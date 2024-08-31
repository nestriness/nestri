import { component$ } from "@builder.io/qwik";
import {
  QwikCityProvider,
  RouterOutlet,
  ServiceWorkerRegister,
} from "@builder.io/qwik-city";
import { RouterHead } from "@nestri/ui";
import { isDev } from "@builder.io/qwik/build";

import "@nestri/ui/globals.css";
import { Fonts } from "@nestri/ui";

export default component$(() => {
  /**
   * The root of a QwikCity site always start with the <QwikCityProvider> component,
   * immediately followed by the document's <head> and <body>.
   *
   * Don't remove the `<head>` and `<body>` elements.
   */

  return (
    <Fonts>
      <QwikCityProvider>
        <head>
          <meta name="theme-color" media="(prefers-color-scheme: light)" content="#fafafa" />
          <meta name="theme-color" media="(prefers-color-scheme: dark)" content="#0a0a0a" />
          <meta charset="utf-8" />
          {!isDev && (
            <link
              rel="manifest"
              href={`${import.meta.env.BASE_URL}manifest.json`}
            />
          )}
          <RouterHead />
        </head>
        <body
          class="bg-gray-50 text-primary-950 dark:bg-gray-950 dark:text-primary-50 font-body flex min-h-[100dvh] flex-col overflow-x-hidden antialiased"
          lang="en">
          <RouterOutlet />
          {/* {!isDev && <ServiceWorkerRegister />} */}
          <ServiceWorkerRegister />
        </body>
      </QwikCityProvider>
    </Fonts>
  );
});
