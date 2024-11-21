package main

import (
	"log"
	"os"
	"os/signal"
	relay "relay/internal"
	"syscall"
)

func main() {
	var err error
	stopCh := make(chan os.Signal, 1)
	signal.Notify(stopCh, os.Interrupt, syscall.SIGTERM)

	// Get flags and log them
	relay.InitFlags()
	relay.GetFlags().DebugLog()

	// Init WebRTC API
	err = relay.InitWebRTCAPI()
	if err != nil {
		log.Fatal("Failed to initialize WebRTC API: ", err)
	}

	// Start our HTTP endpoints
	relay.InitHTTPEndpoint()

	// Wait for exit signal
	<-stopCh
	log.Println("Shutting down gracefully by signal...")
}
