import { Hono } from 'hono'
import Image from "./image"

const app = new Hono()

app.get('/', (c) => {
  return c.text('Hello There! 👋🏾')
})

app.get('/favicon.ico', (c) => {
  return c.newResponse(null, 302, {
    Location: 'https://nestri.pages.dev/favicon.svg'
  })
})

app.route("/image", Image)

export default app