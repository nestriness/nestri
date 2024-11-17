package webrtcrelay

import (
	"encoding/json"
	"errors"
	"fmt"
	"github.com/google/uuid"
	"github.com/pion/webrtc/v4"
	"io"
	"log"
	"net/http"
	"strconv"
	"strings"
)

var httpMux *http.ServeMux

func InitHTTPEndpoint() {
	// Create HTTP mux which serves our WHIP/WHEP endpoints
	httpMux = http.NewServeMux()

	// Endpoints themselves
	httpMux.Handle("/", http.NotFoundHandler())
	httpMux.HandleFunc("/api/whip/{streamName}", corsAnyHandler(whipHandler))
	httpMux.HandleFunc("/api/whep/{streamName}", corsAnyHandler(whepHandler))

	// Get our serving port
	port := GetRelayFlags().EndpointPort

	// Log and start the endpoint server
	log.Println("Starting HTTP endpoint server on :", strconv.Itoa(port))
	go func() {
		log.Fatal((&http.Server{
			Handler: httpMux,
			Addr:    ":" + strconv.Itoa(port),
		}).ListenAndServe())
	}()
}

// WHIP - WebRTC HTTP Ingress Protocol
// This is the handler for the /api/whip/{streamName} endpoint
func whipHandler(w http.ResponseWriter, r *http.Request) {
	// Get stream name
	streamName := r.PathValue("streamName")
	if len(streamName) <= 0 {
		logHTTPError(w, "no stream name given", http.StatusBadRequest)
		return
	}

	// Get body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Get SDP offer from body
	sdpOffer := string(body)
	if sdpOffer == "" {
		// If stream exists, just return OK (force stream close?)
		if _, ok := StreamMap[streamName]; ok {
			w.WriteHeader(http.StatusOK)
			return
		} else {
			logHTTPError(w, "SDP offer not set", http.StatusBadRequest)
			return
		}
	}

	// Verify there's no existing stream with same name
	if _, ok := StreamMap[streamName]; ok {
		logHTTPError(w, "stream name already in use", http.StatusBadRequest)
		return
	}

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetRelayFlags().Verbose {
			log.Println("Closed PeerConnection for stream: ", streamName)
		}
		if _, ok := StreamMap[streamName]; ok {
			delete(StreamMap, streamName)
		}
	}

	// Create a new stream
	if GetRelayFlags().Verbose {
		log.Println("Creating new stream: ", streamName)
	}
	stream := &Stream{}
	stream.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Modify SDP offer to remove opus "sprop-maxcapturerate=24000" (fixes opus bad quality issue, present in GStreamer)
	sdpOffer = strings.Replace(sdpOffer, ";sprop-maxcapturerate=24000", "", -1)

	stream.PeerConnection.OnTrack(func(remoteTrack *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {
		var localTrack *webrtc.TrackLocalStaticRTP
		if remoteTrack.Kind() == webrtc.RTPCodecTypeVideo {
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "video", fmt.Sprint("nestri-", streamName))
			if err != nil {
				log.Println("Failed to create local video track for stream: ", streamName, " - reason: ", err)
				return
			}
			stream.VideoTrack = localTrack
		} else if remoteTrack.Kind() == webrtc.RTPCodecTypeAudio {
			localTrack, err = webrtc.NewTrackLocalStaticRTP(remoteTrack.Codec().RTPCodecCapability, "audio", fmt.Sprint("nestri-", streamName))
			if err != nil {
				log.Println("Failed to create local audio track for stream: ", streamName, " - reason: ", err)
				return
			}
			stream.AudioTrack = localTrack
		}

		// TODO: With custom (non-WHEP) viewer connections, notify them of new stream to set their tracks

		rtpBuffer := make([]byte, 1400)
		for {
			read, _, err := remoteTrack.Read(rtpBuffer)
			if err != nil {
				// EOF is expected when stopping stream
				if !errors.Is(err, io.EOF) {
					log.Println("RTP read error from stream: ", streamName, " - ", err)
				}
				break
			}

			_, err = localTrack.Write(rtpBuffer[:read])
			if err != nil && !errors.Is(err, io.ErrClosedPipe) {
				log.Println("RTP write error from stream: ", streamName, " - ", err)
				break
			}
		}
		// TODO: Cleanup track from viewer for stream restart
	})

	// Set new remote description
	err = stream.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	gatherComplete := webrtc.GatheringCompletePromise(stream.PeerConnection)

	// Create Answer and set local description
	answer, err := stream.PeerConnection.CreateAnswer(nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	err = stream.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	<-gatherComplete

	// Return SDP answer
	w.Header().Set("Content-Type", "application/sdp")
	w.Header().Set("Location", fmt.Sprint("/api/whip/", streamName))
	w.WriteHeader(http.StatusCreated)
	_, err = w.Write([]byte(stream.PeerConnection.LocalDescription().SDP))
	if err != nil {
		log.Println(err)
		return
	}

	// Save to our stream map
	StreamMap[streamName] = stream
}

// WHEP - WebRTC HTTP Egress Protocol
// This is the handler for the /api/whep/{streamName} endpoint
func whepHandler(w http.ResponseWriter, r *http.Request) {
	// Get stream name
	streamName := r.PathValue("streamName")
	if len(streamName) <= 0 {
		logHTTPError(w, "no stream name given", http.StatusBadRequest)
		return
	}

	// Make sure stream exists
	stream, ok := StreamMap[streamName]
	if !ok {
		logHTTPError(w, "no stream with given name online", http.StatusNotFound)
		return
	}

	// Get body
	body, err := io.ReadAll(r.Body)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Get SDP offer from body
	sdpOffer := string(body)
	if sdpOffer == "" {
		logHTTPError(w, "SDP offer not set", http.StatusBadRequest)
		return
	}

	// Generate UUID for viewer
	viewerUUID := uuid.New().String()

	// Callback for closing PeerConnection
	onPCClose := func() {
		if GetRelayFlags().Verbose {
			log.Println("Closed PeerConnection for viewer: ", viewerUUID, " - beloging to stream: ", streamName)
		}
		if _, ok := ViewerMap[streamName]; ok {
			if _, vOk := ViewerMap[viewerUUID]; vOk {
				delete(ViewerMap[streamName], viewerUUID)
			}
		}
	}

	// Create new viewer
	if GetRelayFlags().Verbose {
		log.Println("New viewer for stream: ", streamName)
	}
	viewer := &Viewer{
		UUID: viewerUUID,
	}
	viewer.PeerConnection, err = CreatePeerConnection(onPCClose)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusInternalServerError)
		return
	}

	// Add stream tracks for viewer
	if stream.AudioTrack != nil {
		if err = viewer.AddTrack(&stream.AudioTrack); err != nil {
			logHTTPError(w, "failed to add audio track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil audio track for stream: ", streamName)
	}
	if stream.VideoTrack != nil {
		if err = viewer.AddTrack(&stream.VideoTrack); err != nil {
			logHTTPError(w, "failed to add video track to viewer", http.StatusInternalServerError)
			return
		}
	} else if GetRelayFlags().Verbose {
		log.Println("nil video track for stream: ", streamName)
	}

	// Set new remote description
	err = viewer.PeerConnection.SetRemoteDescription(webrtc.SessionDescription{
		Type: webrtc.SDPTypeOffer,
		SDP:  sdpOffer,
	})
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	gatherComplete := webrtc.GatheringCompletePromise(viewer.PeerConnection)

	// Create Answer and set local description
	answer, err := viewer.PeerConnection.CreateAnswer(nil)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	err = viewer.PeerConnection.SetLocalDescription(answer)
	if err != nil {
		logHTTPError(w, err.Error(), http.StatusBadRequest)
		return
	}

	// Wait for ICE Gathering to complete
	<-gatherComplete

	// Return SDP answer
	w.Header().Set("Content-Type", "application/sdp")
	w.Header().Set("Location", fmt.Sprint("/api/whep/", streamName))
	w.WriteHeader(http.StatusCreated)
	_, err = w.Write([]byte(viewer.PeerConnection.LocalDescription().SDP))
	if err != nil {
		log.Println(err)
		return
	}
}

// logHTTPError logs (if verbose) and sends an error code to requester
func logHTTPError(w http.ResponseWriter, err string, code int) {
	if GetRelayFlags().Verbose {
		log.Println(err)
	}
	http.Error(w, err, code)
}

// httpError sends a web response with an error message
func httpError(w http.ResponseWriter, statusCode int, message string) {
	w.WriteHeader(statusCode)
	_ = json.NewEncoder(w).Encode(map[string]string{"error": message})
}

// respondWithJSON writes JSON to the response body
func respondWithJSON(w http.ResponseWriter, statusCode int, payload interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	_ = json.NewEncoder(w).Encode(payload)
}

// respondWithText writes text to the response body
func respondWithText(w http.ResponseWriter, statusCode int, payload string) {
	w.Header().Set("Content-Type", "text/plain")
	w.WriteHeader(statusCode)
	_, _ = w.Write([]byte(payload))
}

// corsAnyHandler allows any origin to access the endpoint
func corsAnyHandler(next func(w http.ResponseWriter, r *http.Request)) http.HandlerFunc {
	return func(res http.ResponseWriter, req *http.Request) {
		// Allow all origins
		res.Header().Set("Access-Control-Allow-Origin", "*")
		res.Header().Set("Access-Control-Allow-Methods", "*")
		res.Header().Set("Access-Control-Allow-Headers", "*")

		if req.Method != http.MethodOptions {
			next(res, req)
		}
	}
}
