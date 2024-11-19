package relay

import (
	"log"
	"net/http"
	"strconv"
)

var httpMux *http.ServeMux

func InitHTTPEndpoint() {
	// Create HTTP mux which serves our WHIP/WHEP endpoints
	httpMux = http.NewServeMux()

	// Endpoints themselves
	httpMux.Handle("/", http.NotFoundHandler())
	httpMux.HandleFunc("/api/whip/{roomName}", corsAnyHandler(whipHandler))
	httpMux.HandleFunc("/api/whep/{roomName}", corsAnyHandler(whepHandler))

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

// logHTTPError logs (if verbose) and sends an error code to requester
func logHTTPError(w http.ResponseWriter, err string, code int) {
	if GetRelayFlags().Verbose {
		log.Println(err)
	}
	http.Error(w, err, code)
}

// // httpError sends a web response with an error message
// func httpError(w http.ResponseWriter, statusCode int, message string) {
// 	w.WriteHeader(statusCode)
// 	_ = json.NewEncoder(w).Encode(map[string]string{"error": message})
// }

// // respondWithJSON writes JSON to the response body
// func respondWithJSON(w http.ResponseWriter, statusCode int, payload interface{}) {
// 	w.Header().Set("Content-Type", "application/json")
// 	w.WriteHeader(statusCode)
// 	_ = json.NewEncoder(w).Encode(payload)
// }

// // respondWithText writes text to the response body
// func respondWithText(w http.ResponseWriter, statusCode int, payload string) {
// 	w.Header().Set("Content-Type", "text/plain")
// 	w.WriteHeader(statusCode)
// 	_, _ = w.Write([]byte(payload))
// }

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
