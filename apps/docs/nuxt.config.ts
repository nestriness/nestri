import { createResolver, logger, defineNuxtModule } from '@nuxt/kit'
import { $fetch } from 'ofetch'
import { version } from './package.json'

const { resolve } = createResolver(import.meta.url)

// That allows to overwrite these dependencies paths via `.env` for local development
const envModules = {
  tokens: process?.env?.THEME_DEV_TOKENS_PATH || '@nuxt-themes/tokens',
  elements: process?.env?.THEME_DEV_ELEMENTS_PATH || '@nuxt-themes/elements',
  studio: process?.env?.THEME_DEV_STUDIO_PATH || '@nuxthq/studio',
  typography: process?.env?.THEME_DEV_TYPOGRAPHY_PATH || '@nuxt-themes/typography'
}

const updateModule = defineNuxtModule({
  meta: {
    name: '@nuxt-themes/docus'
  },
  setup (_, nuxt) {
    if (nuxt.options.dev) {
      $fetch('https://registry.npmjs.org/@nuxt-themes/docus/latest').then((release) => {
        if (release.version > version) {
          logger.info(`A new version of Docus (v${release.version}) is available: https://github.com/nuxt-themes/docus/releases/latest`)
        }
      }).catch(() => {})
    }
  }
})

export default defineNuxtConfig({
  // https://github.com/nuxt-themes/docus
  extends: ['@nuxt-themes/docus'],

  devtools: { enabled: true },



  compatibilityDate: '2024-09-26'
})