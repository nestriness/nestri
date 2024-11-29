mod args;
mod enc_helper;
mod gpu;
mod room;
mod websocket;
mod latency;
mod messages;

use crate::args::encoding_args;
use gst::prelude::*;
use gst_app::AppSink;
use std::error::Error;
use std::str::FromStr;
use std::sync::Arc;
use futures_util::StreamExt;
use gst_app::app_sink::AppSinkStream;
use tokio::sync::{Mutex};
use crate::websocket::{NestriWebSocket};

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
// TODO: Expand enc_helper with audio types, for now just opus
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
    // Parse command line arguments
    let args = args::Args::new();
    if args.app.verbose {
        args.debug_print();
    }

    rustls::crypto::ring::default_provider()
        .install_default()
        .expect("Failed to install ring crypto provider");

    // Begin connection attempt to the relay WebSocket endpoint
    // replace any http/https with ws/wss
    let replaced_relay_url
        = args.app.relay_url.replace("http://", "ws://").replace("https://", "wss://");
    let ws_url = format!(
        "{}/api/ws/{}",
        replaced_relay_url,
        args.app.room,
    );

    // Setup our websocket
    let nestri_ws = Arc::new(NestriWebSocket::new(ws_url).await?);
    log::set_max_level(log::LevelFilter::Info);
    log::set_boxed_logger(Box::new(nestri_ws.clone())).unwrap();

    let _ = gst::init();

    // Handle GPU selection
    let gpu = handle_gpus(&args);
    if gpu.is_none() {
        log::error!("Failed to find a suitable GPU. Exiting..");
        return Err("Failed to find a suitable GPU. Exiting..".into());
    }
    let gpu = gpu.unwrap();

    // Handle video encoder selection
    let video_encoder_info = handle_encoder_video(&args);
    if video_encoder_info.is_none() {
        log::error!("Failed to find a suitable video encoder. Exiting..");
        return Err("Failed to find a suitable video encoder. Exiting..".into());
    }
    let mut video_encoder_info = video_encoder_info.unwrap();
    // Handle video encoder settings
    video_encoder_info = handle_encoder_video_settings(&args, &video_encoder_info);

    // Handle audio encoder selection
    let audio_encoder = handle_encoder_audio(&args);

    /*** ROOM SETUP ***/
    let room = Arc::new(Mutex::new(
        room::Room::new(nestri_ws.clone()).await?,
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
    
    // Required to fix gstreamer opus issue, where quality sounds off (due to wrong sample rate)
    let audio_capsfilter = gst::ElementFactory::make("capsfilter").build()?;
    let audio_caps = gst::Caps::from_str("audio/x-raw,rate=48000").unwrap();
    audio_capsfilter.set_property("caps", &audio_caps);

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
    let audio_appsink = audio_appsink.downcast_ref::<AppSink>().unwrap();

    // Video AppSink Element
    let video_appsink = gst::ElementFactory::make("appsink").build()?;
    video_appsink.set_property("emit-signals", &true);
    let video_appsink = video_appsink.downcast_ref::<AppSink>().unwrap();

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
    debug_queue.set_property("max-size-time", &1000000u64);
    let main_video_queue = gst::ElementFactory::make("queue2").build()?;
    main_video_queue.set_property("max-size-time", &1000000u64);
    let main_audio_queue = gst::ElementFactory::make("queue2").build()?;
    main_audio_queue.set_property("max-size-time", &1000000u64);

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
        &audio_capsfilter,
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
        &audio_capsfilter,
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

    // Optimize latency of pipeline
    video_source.set_property("do-timestamp", &true);
    audio_source.set_property("do-timestamp", &true);
    pipeline.set_property("latency", &0u64);

    // Wrap the pipeline in Arc<Mutex> to safely share it
    let pipeline = Arc::new(Mutex::new(pipeline));

    // Run both pipeline and websocket tasks concurrently
    let result = tokio::try_join!(
        run_room(
            room.clone(),
            "audio/opus",
            video_encoder_info.codec.to_mime_str(),
            pipeline.clone(),
            Arc::new(Mutex::new(audio_appsink.stream())),
            Arc::new(Mutex::new(video_appsink.stream()))
        ),
        run_pipeline(pipeline.clone())
    );

    match result {
        Ok(_) => log::info!("All tasks completed successfully"),
        Err(e) => {
            log::error!("Error occurred in one of the tasks: {}", e);
            return Err("Error occurred in one of the tasks".into());
        }
    }

    Ok(())
}

async fn run_room(
    room: Arc<Mutex<room::Room>>,
    audio_codec: &str,
    video_codec: &str,
    pipeline: Arc<Mutex<gst::Pipeline>>,
    audio_stream: Arc<Mutex<AppSinkStream>>,
    video_stream: Arc<Mutex<AppSinkStream>>,
) -> Result<(), Box<dyn Error>> {
    // Run loop, with recovery on error
    loop {
        let mut room = room.lock().await;
        tokio::select! {
            _ = tokio::signal::ctrl_c() => {
                log::info!("Room interrupted via Ctrl+C");
                return Ok(());
            }
            result = room.run(
                audio_codec,
                video_codec,
                pipeline.clone(),
                audio_stream.clone(),
                video_stream.clone(),
            ) => {
                if let Err(e) = result {
                    log::error!("Room error: {}", e);
                    // Sleep for a while before retrying
                    tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
                } else {
                    return Ok(());
                }
            }
        }
    }
}

async fn run_pipeline(
    pipeline: Arc<Mutex<gst::Pipeline>>,
) -> Result<(), Box<dyn Error>> {
    // Take ownership of the bus without holding the lock
    let bus = {
        let pipeline = pipeline.lock().await;
        pipeline.bus().ok_or("Pipeline has no bus")?
    };

    {
        // Temporarily lock the pipeline to change state
        let pipeline = pipeline.lock().await;
        if let Err(e) = pipeline.set_state(gst::State::Playing) {
            log::error!("Failed to start pipeline: {}", e);
            return Err("Failed to start pipeline".into());
        }
    }

    // Wait for EOS or error (don't lock the pipeline indefinitely)
    tokio::select! {
        _ = tokio::signal::ctrl_c() => {
            log::info!("Pipeline interrupted via Ctrl+C");
        }
        result = listen_for_gst_messages(bus) => {
            match result {
                Ok(_) => log::info!("Pipeline finished with EOS"),
                Err(err) => log::error!("Pipeline error: {}", err),
            }
        }
    }

    {
        // Temporarily lock the pipeline to reset state
        let pipeline = pipeline.lock().await;
        pipeline.set_state(gst::State::Null)?;
    }

    Ok(())
}

async fn listen_for_gst_messages(bus: gst::Bus) -> Result<(), Box<dyn Error>> {
    let bus_stream = bus.stream();

    tokio::pin!(bus_stream);

    while let Some(msg) = bus_stream.next().await {
        match msg.view() {
            gst::MessageView::Eos(_) => {
                log::info!("Received EOS");
                break;
            }
            gst::MessageView::Error(err) => {
                let err_msg = format!(
                    "Error from {:?}: {:?}",
                    err.src().map(|s| s.path_string()),
                    err.error()
                );
                return Err(err_msg.into());
            }
            _ => (),
        }
    }

    Ok(())
}
