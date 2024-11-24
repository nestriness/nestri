mod args;
mod enc_helper;
mod gpu;

use crate::args::encoding_args;
use gst::prelude::*;
use gst_app::AppSink;
use std::collections::VecDeque;
use std::error::Error;
use std::str::FromStr;
use std::sync::Arc;
use tokio::sync::{mpsc, Mutex, Notify};
mod room;

// Handles gathering GPU information and selecting the most suitable GPU
fn handle_gpus(args: &args::Args) -> Option<gpu::GPUInfo> {
    println!("Gathering GPU information..");
    let gpus = gpu::get_gpus();
    if gpus.is_empty() {
        println!("No GPUs found");
        return None;
    }
    for gpu in &gpus {
        println!(
            "> [GPU] Vendor: '{}', Card Path: '{}', Render Path: '{}', Device Name: '{}'",
            gpu.vendor_string(),
            gpu.card_path(),
            gpu.render_path(),
            gpu.device_name()
        );
    }

    // Based on available arguments, pick a GPU
    let gpu;
    if !args.device.gpu_card_path.is_empty() {
        gpu = gpu::get_gpu_by_card_path(&gpus, &args.device.gpu_card_path);
    } else {
        // Run all filters that are not empty
        let mut filtered_gpus = gpus.clone();
        if !args.device.gpu_vendor.is_empty() {
            filtered_gpus = gpu::get_gpus_by_vendor(&filtered_gpus, &args.device.gpu_vendor);
        }
        if !args.device.gpu_name.is_empty() {
            filtered_gpus = gpu::get_gpus_by_device_name(&filtered_gpus, &args.device.gpu_name);
        }
        if args.device.gpu_index != 0 {
            // get single GPU by index
            gpu = filtered_gpus.get(args.device.gpu_index as usize).cloned();
        } else {
            // get first GPU
            gpu = filtered_gpus.get(0).cloned();
        }
    }
    if gpu.is_none() {
        println!("No GPU found with the specified parameters: vendor='{}', name='{}', index='{}', card_path='{}'",
                 args.device.gpu_vendor, args.device.gpu_name, args.device.gpu_index, args.device.gpu_card_path);
        return None;
    }
    let gpu = gpu.unwrap();
    println!("Selected GPU: '{}'", gpu.device_name());
    Some(gpu)
}

// Handles picking video encoder
fn handle_encoder_video(args: &args::Args) -> Option<enc_helper::VideoEncoderInfo> {
    println!("Getting compatible video encoders..");
    let video_encoders = enc_helper::get_compatible_encoders();
    if video_encoders.is_empty() {
        println!("No compatible video encoders found");
        return None;
    }
    for encoder in &video_encoders {
        println!(
            "> [Video Encoder] Name: '{}', Codec: '{}', API: '{}', Type: '{}'",
            encoder.name,
            encoder.codec.to_str(),
            encoder.encoder_api.to_str(),
            encoder.encoder_type.to_str()
        );
    }
    // Pick most suitable video encoder based on given arguments
    let video_encoder;
    if !args.encoding.video.encoder.is_empty() {
        video_encoder =
            enc_helper::get_encoder_by_name(&video_encoders, &args.encoding.video.encoder);
    } else {
        video_encoder = enc_helper::get_best_compatible_encoder(
            &video_encoders,
            enc_helper::VideoCodec::from_str(&args.encoding.video.codec),
            enc_helper::EncoderType::from_str(&args.encoding.video.encoder_type),
        );
    }
    if video_encoder.is_none() {
        println!("No video encoder found with the specified parameters: name='{}', vcodec='{}', type='{}'",
                 args.encoding.video.encoder, args.encoding.video.codec, args.encoding.video.encoder_type);
        return None;
    }
    let video_encoder = video_encoder.unwrap();
    println!("Selected video encoder: '{}'", video_encoder.name);
    Some(video_encoder)
}

// Handles picking preferred settings for video encoder
fn handle_encoder_video_settings(
    args: &args::Args,
    video_encoder: &enc_helper::VideoEncoderInfo,
) -> enc_helper::VideoEncoderInfo {
    let mut optimized_encoder = enc_helper::encoder_low_latency_params(&video_encoder);
    // Handle rate-control method
    match &args.encoding.video.rate_control {
        encoding_args::RateControl::CQP(cqp) => {
            optimized_encoder = enc_helper::encoder_cqp_params(&optimized_encoder, cqp.quality);
        }
        encoding_args::RateControl::VBR(vbr) => {
            optimized_encoder = enc_helper::encoder_vbr_params(
                &optimized_encoder,
                vbr.target_bitrate as u32,
                vbr.max_bitrate as u32,
            );
        }
        encoding_args::RateControl::CBR(cbr) => {
            optimized_encoder =
                enc_helper::encoder_cbr_params(&optimized_encoder, cbr.target_bitrate as u32);
        }
    }
    println!(
        "Selected video encoder settings: '{}'",
        optimized_encoder.get_parameters_string()
    );
    optimized_encoder
}

// Handles picking audio encoder
// TODO: Expand enc_helper with audio types, for now just AAC or opus
fn handle_encoder_audio(args: &args::Args) -> String {
    let audio_encoder = if args.encoding.audio.encoder.is_empty() {
        "opusenc".to_string()
    } else {
        args.encoding.audio.encoder.clone()
    };
    println!("Selected audio encoder: '{}'", audio_encoder);
    audio_encoder
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    let args = args::Args::new();
    if args.app.verbose {
        args.debug_print();
    }

    rustls::crypto::ring::default_provider()
        .install_default()
        .expect("Failed to install ring crypto provider");

    let _ = gst::init();

    // Handle GPU selection
    let gpu = handle_gpus(&args);
    if gpu.is_none() {
        println!("Failed to find a suitable GPU. Exiting..");
        return Ok(());
    }
    let gpu = gpu.unwrap();

    // Handle video encoder selection
    let video_encoder_info = handle_encoder_video(&args);
    if video_encoder_info.is_none() {
        println!("Failed to find a suitable video encoder. Exiting..");
        return Ok(());
    }
    let mut video_encoder_info = video_encoder_info.unwrap();
    // Handle video encoder settings
    video_encoder_info = handle_encoder_video_settings(&args, &video_encoder_info);

    // Handle audio encoder selection
    let audio_encoder = handle_encoder_audio(&args);

    /*** ROOM SETUP ***/
    // Set up buffers for pipeline and room communication
    let input_notify = Arc::new(Notify::new());
    let input_buffer = Arc::new(Mutex::new(VecDeque::with_capacity(50)));
    let audio_buffer = Arc::new(Mutex::new(VecDeque::with_capacity(50)));
    let video_buffer = Arc::new(Mutex::new(VecDeque::with_capacity(50)));

    // Set relay url
    let relay_url = format!(
        "
        {}/api/whip/{}
        ",
        args.app.relay_url, args.app.room
    );
    let room = Arc::new(Mutex::new(
        room::Room::new(
            relay_url,
            input_notify.clone(),
            input_buffer.clone(),
            audio_buffer.clone(),
            video_buffer.clone(),
        )
        .await?,
    ));

    /*** PIPELINE CREATION ***/
    /* Audio */
    // Audio Source Element
    let audio_source = match args.encoding.audio.capture_method {
        encoding_args::AudioCaptureMethod::PulseAudio => {
            gst::ElementFactory::make("pulsesrc").build()?
        }
        encoding_args::AudioCaptureMethod::PipeWire => {
            gst::ElementFactory::make("pipewiresrc").build()?
        }
        _ => gst::ElementFactory::make("alsasrc").build()?,
    };

    // Audio Converter Element
    let audio_converter = gst::ElementFactory::make("audioconvert").build()?;

    // Audio Encoder Element
    let audio_encoder = gst::ElementFactory::make(audio_encoder.as_str()).build()?;
    audio_encoder.set_property(
        "bitrate",
        &match &args.encoding.audio.rate_control {
            encoding_args::RateControl::CBR(cbr) => cbr.target_bitrate * 1000i32,
            encoding_args::RateControl::VBR(vbr) => vbr.target_bitrate * 1000i32,
            _ => 128i32,
        },
    );

    // Audio RTP Payloader Element
    let audio_rtp_payloader = gst::ElementFactory::make("rtpopuspay").build()?;

    /* Video */
    // Video Source Element
    let video_source = gst::ElementFactory::make("waylanddisplaysrc").build()?;
    video_source.set_property("render-node", &gpu.render_path());

    // Caps Filter Element (resolution, fps)
    let caps_filter = gst::ElementFactory::make("capsfilter").build()?;
    let caps = gst::Caps::from_str(&format!(
        "video/x-raw,width={},height={},framerate={}/1,format=RGBx",
        args.app.resolution.0, args.app.resolution.1, args.app.framerate
    ))?;
    caps_filter.set_property("caps", &caps);

    // Video Tee Element
    let video_tee = gst::ElementFactory::make("tee").build()?;

    // Video Converter Element
    let video_converter = gst::ElementFactory::make("videoconvert").build()?;

    // Video Encoder Element
    let video_encoder = gst::ElementFactory::make(video_encoder_info.name.as_str()).build()?;
    video_encoder_info.apply_parameters(&video_encoder, &args.app.verbose);

    // Required for AV1 - av1parse
    let av1_parse = gst::ElementFactory::make("av1parse").build()?;

    // Video RTP Payloader Element
    let video_rtp_payloader = gst::ElementFactory::make(
        format!("rtp{}pay", video_encoder_info.codec.to_gst_str()).as_str(),
    )
    .build()?;

    /* Output */
    // Audio AppSink Element
    let audio_appsink = gst::ElementFactory::make("appsink").build()?;
    audio_appsink.set_property("emit-signals", &true);
    // Handle new-sample samples
    let audio_sink_buf = audio_buffer.clone();
    let audio_appsink_clone = audio_appsink.clone();
    tokio::spawn(async move {
        let appsink = audio_appsink_clone.downcast_ref::<AppSink>().unwrap();
        loop {
            if appsink.is_eos() {
                continue
            }
            let sample = match appsink.pull_sample() {
                Ok(sample) => sample,
                Err(e) => {
                    println!("Audio AppSink error: {}", e);
                    break
                }
            };
            let buffer = sample.buffer().unwrap();
            let buffer = buffer.map_readable().unwrap();
            let data = buffer.as_slice();
            let mut audio_sink_buf = audio_sink_buf.lock().await;
            // If at capacity, remove oldest element
            if audio_sink_buf.len() >= audio_sink_buf.capacity() {
                audio_sink_buf.pop_front();
            }
            audio_sink_buf.push_back(data.to_vec());
        }
    });

    // Video AppSink Element
    let video_appsink = gst::ElementFactory::make("appsink").build()?;
    video_appsink.set_property("emit-signals", &true);
    // Handle new-sample samples
    let video_sink_buf = video_buffer.clone();
    let video_appsink_clone = video_appsink.clone();
    tokio::spawn(async move {
        let appsink = video_appsink_clone.downcast_ref::<AppSink>().unwrap();
        loop {
            if appsink.is_eos() {
                continue
            }
            let sample = match appsink.pull_sample() {
                Ok(sample) => sample,
                Err(e) => {
                    println!("Video AppSink error: {}", e);
                    break
                }
            };
            let buffer = sample.buffer().unwrap();
            let buffer = buffer.map_readable().unwrap();
            let data = buffer.as_slice();
            let mut video_sink_buf = video_sink_buf.lock().await;
            // If at capacity, remove oldest element
            if video_sink_buf.len() >= video_sink_buf.capacity() {
                video_sink_buf.pop_front();
            }
            video_sink_buf.push_back(data.to_vec());
        }
    });

    /* Debug */
    // Debug Feed Element
    let debug_latency = gst::ElementFactory::make("timeoverlay").build()?;
    debug_latency.set_property_from_str("halignment", &"right");
    debug_latency.set_property_from_str("valignment", &"bottom");

    // Debug Sink Element
    let debug_sink = gst::ElementFactory::make("ximagesink").build()?;

    // Debug video converter
    let debug_video_converter = gst::ElementFactory::make("videoconvert").build()?;

    // Queues with max 2ms latency
    let debug_queue = gst::ElementFactory::make("queue2").build()?;
    debug_queue.set_property("max-size-time", &2000000u64);
    let main_video_queue = gst::ElementFactory::make("queue2").build()?;
    main_video_queue.set_property("max-size-time", &2000000u64);
    let main_audio_queue = gst::ElementFactory::make("queue2").build()?;
    main_audio_queue.set_property("max-size-time", &2000000u64);

    // Create the pipeline
    let pipeline = gst::Pipeline::new();

    // Add elements to the pipeline
    pipeline.add_many(&[
        &video_appsink.upcast_ref(),
        &video_rtp_payloader,
        &video_encoder,
        &video_converter,
        &video_tee,
        &caps_filter,
        &video_source,
        &audio_appsink.upcast_ref(),
        &audio_rtp_payloader,
        &audio_encoder,
        &audio_converter,
        &audio_source,
        &main_video_queue,
        &main_audio_queue,
    ])?;

    // Add debug elements if debug is enabled
    if args.app.debug_feed {
        pipeline.add_many(&[&debug_sink, &debug_queue, &debug_video_converter])?;
    }

    // Add debug latency element if debug latency is enabled
    if args.app.debug_latency {
        pipeline.add(&debug_latency)?;
    }

    // Add AV1 parse element if AV1 is selected
    if video_encoder_info.codec == enc_helper::VideoCodec::AV1 {
        pipeline.add(&av1_parse)?;
    }

    // Link main audio branch
    gst::Element::link_many(&[
        &audio_source,
        &audio_converter,
        &audio_encoder,
        &audio_rtp_payloader,
        &main_audio_queue,
        &audio_appsink.upcast_ref(),
    ])?;

    // If debug latency, add time overlay before tee
    if args.app.debug_latency {
        gst::Element::link_many(&[&video_source, &caps_filter, &debug_latency, &video_tee])?;
    } else {
        gst::Element::link_many(&[&video_source, &caps_filter, &video_tee])?;
    }

    // Link debug branch if debug is enabled
    if args.app.debug_feed {
        gst::Element::link_many(&[
            &video_tee,
            &debug_video_converter,
            &debug_queue,
            &debug_sink,
        ])?;
    }

    // Link main video branch, if AV1, add av1_parse
    if video_encoder_info.codec == enc_helper::VideoCodec::AV1 {
        gst::Element::link_many(&[
            &video_tee,
            &video_converter,
            &video_encoder,
            &av1_parse,
            &video_rtp_payloader,
            &main_video_queue,
            &video_appsink.upcast_ref(),
        ])?;
    } else {
        gst::Element::link_many(&[
            &video_tee,
            &video_converter,
            &video_encoder,
            &video_rtp_payloader,
            &main_video_queue,
            &video_appsink.upcast_ref(),
        ])?;
    }

    // Wrap the pipeline in Arc<Mutex> to safely share it
    let pipeline = Arc::new(Mutex::new(pipeline));

    // Run both pipeline and websocket tasks concurrently
    let result = tokio::try_join!(
        run_room(room.clone(),
            "audio/opus",
            video_encoder_info.codec.to_mime_str(),
        ),
        run_pipeline(pipeline.clone(), input_notify.clone(), input_buffer.clone())
    );

    match result {
        Ok(_) => println!("Both tasks completed successfully."),
        Err(e) => {
            eprintln!("One of the tasks failed: {} - exiting", e);
            // Exit immediately
            std::process::exit(1);
        }
    }

    Ok(())
}

async fn run_room(
    room: Arc<Mutex<room::Room>>,
    audio_codec: &str,
    video_codec: &str,
) -> Result<(), Box<dyn Error>> {
    // Run loop, with recovery on error
    loop {
        let mut room = room.lock().await;
        if let Err(e) = room.run(audio_codec, video_codec).await {
            eprintln!("Room error: {}", e);
            // Sleep for a while before retrying
            tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
        }
    }
}

async fn run_pipeline(
    pipeline: Arc<Mutex<gst::Pipeline>>,
    input_notify: Arc<Notify>,
    input_buffer: Arc<Mutex<VecDeque<gst::Event>>>,
) -> Result<(), Box<dyn Error>> {
    // Set the pipeline state to Playing when starting the pipeline
    if let Err(e) = pipeline.lock().await.set_state(gst::State::Playing) {
        return Err(format!("Failed to set pipeline state to Playing: {}", e).into());
    }

    // Spawn the error handling task
    let (error_tx, mut error_rx) = mpsc::channel(1);
    let pipeline_clone = pipeline.clone();
    tokio::spawn(async move {
        if let Err(e) = handle_pipeline_errors(pipeline_clone, error_tx).await {
            eprintln!("Error handling pipeline errors: {}", e);
        }
    });

    // Wait for the event task to complete or an error to occur
    loop {
        tokio::select! {
            _ = input_notify.notified() => {
                let mut buf =  input_buffer.lock().await;
                while let Some(event) = buf.pop_front() {
                    let _ = pipeline.lock().await.send_event(event); // Ignore success value
                }
            }
            Some(_) = error_rx.recv() => {
                eprintln!("Pipeline error occurred. Stopping..");
                break;
            }
        }
    }

    Ok(())
}

async fn handle_pipeline_errors(
    pipeline: Arc<Mutex<gst::Pipeline>>,
    error_tx: mpsc::Sender<()>,
) -> Result<(), Box<dyn Error>> {
    let bus = pipeline
        .lock()
        .await
        .bus()
        .expect("Pipeline without bus. Shouldn't happen!");
    for msg in bus.iter_timed(gst::ClockTime::NONE) {
        use gst::MessageView;
        match msg.view() {
            MessageView::Eos(..) => {
                println!("Pipeline reached EOS.");
                break;
            }
            MessageView::Error(err) => {
                let _ = pipeline.lock().await.set_state(gst::State::Null);
                eprintln!(
                    "Pipeline error: {} ({})",
                    err.error(),
                    err.debug().unwrap_or_else(|| "".into())
                );
                let _ = error_tx.send(()).await;
                break;
            }
            _ => (),
        }
    }
    Ok(())
}
