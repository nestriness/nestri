// Utility function to download a Uint8Array for debugging.
export function download(data: Uint8Array, name: string) {
	const blob = new Blob([data], {
		type: "application/octet-stream",
	})

	const url = window.URL.createObjectURL(blob)

	const a = document.createElement("a")
	a.href = url
	a.download = name
	document.body.appendChild(a)
	a.style.display = "none"
	a.click()
	a.remove()

	setTimeout(() => window.URL.revokeObjectURL(url), 1000)
}
