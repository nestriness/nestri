package relay

import (
	"log"
	"sync"

	"github.com/pion/interceptor"
	"github.com/pion/webrtc/v4"
)

type Room struct {
	PeerConnection *webrtc.PeerConnection
	AudioTrack     webrtc.TrackLocal
	VideoTrack     webrtc.TrackLocal
	participants   map[*Participant]bool
	broadcast      chan string
	mutex          sync.RWMutex
	name           string
}

type Participant struct {
	name           string
	room           *Room
	send           chan string
	PeerConnection *webrtc.PeerConnection
}

func (vw *Participant) AddTrack(trackLocal *webrtc.TrackLocal) error {
	rtpSender, err := vw.PeerConnection.AddTrack(*trackLocal)
	if err != nil {
		return err
	}

	go func() {
		rtcpBuffer := make([]byte, 1400)
		for {
			if _, _, rtcpErr := rtpSender.Read(rtcpBuffer); rtcpErr != nil {
				return
			}
		}
	}()

	return nil
}

var Rooms = make(map[string]*Room)                          //< room name -> room
var Participants = make(map[string]map[string]*Participant) //< room name -> participants by their names
var mutex sync.RWMutex

var globalWebRTCAPI *webrtc.API
var globalWebRTCConfig = webrtc.Configuration{
	ICETransportPolicy: webrtc.ICETransportPolicyAll,
	BundlePolicy:       webrtc.BundlePolicyBalanced,
	SDPSemantics:       webrtc.SDPSemanticsUnifiedPlan,
}

func InitWebRTCAPI() error {
	// Make our maps

	var err error
	flags := GetRelayFlags()

	// Media engine
	mediaEngine := &webrtc.MediaEngine{}

	// Default codecs cover most of our needs
	err = mediaEngine.RegisterDefaultCodecs()
	if err != nil {
		return err
	}

	// Interceptor registry
	interceptorRegistry := &interceptor.Registry{}

	// Use default set
	err = webrtc.RegisterDefaultInterceptors(mediaEngine, interceptorRegistry)
	if err != nil {
		return err
	}

	// Setting engine
	settingEngine := webrtc.SettingEngine{}

	// New in v4, reduces CPU usage and latency when enabled
	settingEngine.EnableSCTPZeroChecksum(true)

	// Set the UDP port range used by WebRTC
	err = settingEngine.SetEphemeralUDPPortRange(uint16(flags.WebRTCUDPStart), uint16(flags.WebRTCUDPEnd))
	if err != nil {
		return err
	}

	// Create a new API object with our customized settings
	globalWebRTCAPI = webrtc.NewAPI(webrtc.WithMediaEngine(mediaEngine), webrtc.WithSettingEngine(settingEngine), webrtc.WithInterceptorRegistry(interceptorRegistry))

	return nil
}

// GetWebRTCAPI returns the global WebRTC API
func GetWebRTCAPI() *webrtc.API {
	return globalWebRTCAPI
}

// CreatePeerConnection sets up a new peer connection
func CreatePeerConnection(onClose func()) (*webrtc.PeerConnection, error) {
	pc, err := globalWebRTCAPI.NewPeerConnection(globalWebRTCConfig)
	if err != nil {
		return nil, err
	}

	// Log connection state changes and handle failed/disconnected connections
	pc.OnConnectionStateChange(func(connectionState webrtc.PeerConnectionState) {
		// Close PeerConnection in cases
		if connectionState == webrtc.PeerConnectionStateFailed ||
			connectionState == webrtc.PeerConnectionStateDisconnected ||
			connectionState == webrtc.PeerConnectionStateClosed {
			err := pc.Close()
			if err != nil {
				log.Printf("Error closing PeerConnection: %s\n", err.Error())
			}
			onClose()
		}
	})

	return pc, nil
}
