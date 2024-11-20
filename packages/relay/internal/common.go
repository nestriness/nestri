package relay

import (
	"fmt"
	"log"
	"time"

	"github.com/pion/interceptor"
	"github.com/pion/randutil"
	"github.com/pion/webrtc/v4"
)

type Room struct {
	PeerConnection *webrtc.PeerConnection
	AudioTrack     webrtc.TrackLocal
	VideoTrack     webrtc.TrackLocal
	name           string
	participants   map[*Participant]bool
}

type Participant struct {
	name           string
	DataChannel    *webrtc.DataChannel
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

func (r *Room) listenToParticipants() {
	println("Listening to participants")

	for participant := range r.participants {
		println("Listening to participant %s for data channels", participant.name)

		participant.PeerConnection.OnDataChannel(func(d *webrtc.DataChannel) {
			fmt.Printf("New DataChannel %s %d\n", d.Label(), d.ID())

			// Register channel opening handling
			d.OnOpen(func() {
				fmt.Printf("Data channel '%s'-'%d' open. Random messages will now be sent to any connected DataChannels every 5 seconds\n", d.Label(), d.ID())

				ticker := time.NewTicker(5 * time.Second)
				defer ticker.Stop()
				for range ticker.C {
					message, sendErr := randutil.GenerateCryptoRandomString(15, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")
					if sendErr != nil {
						panic(sendErr)
					}

					// Send the message as text
					fmt.Printf("Sending '%s'\n", message)
					if sendErr = d.SendText(message); sendErr != nil {
						panic(sendErr)
					}
				}
			})

			// Register text message handling
			d.OnMessage(func(msg webrtc.DataChannelMessage) {
				fmt.Printf("Message from DataChannel '%s': '%s'\n", d.Label(), string(msg.Data))
			})
		})

		// for range time.Tick(time.Second * 1) {
		// 	fmt.Println("Still working")
		// }
	}

	select {}
}
