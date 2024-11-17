package main

import (
	"log"
	"os"
	"os/signal"
	"syscall"
	"webrtcrelay/internal"
)

func main() {
	var err error
	stopCh := make(chan os.Signal, 1)
	signal.Notify(stopCh, os.Interrupt, syscall.SIGTERM)

	// Get flags and log them
	webrtcrelay.InitRelayFlags()
	webrtcrelay.GetRelayFlags().DebugLog()

	// Init WebRTC API
	err = webrtcrelay.InitWebRTCAPI()
	if err != nil {
		log.Fatal("Failed to initialize WebRTC API: ", err)
	}

	// Start our HTTP endpoints
	webrtcrelay.InitHTTPEndpoint()

	// Wait for exit signal
	<-stopCh
	log.Println("Shutting down gracefully by signal...")
}
