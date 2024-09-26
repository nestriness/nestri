import { component$ } from "@builder.io/qwik";
import { useDocumentHead, useLocation } from "@builder.io/qwik-city";

/**
 * The RouterHead component is placed inside of the document `<head>` element.
 */
export const RouterHead = component$(() => {
  const head = useDocumentHead();
  const loc = useLocation();
  const domain = loc.url.origin;

  return (
    <>
      <title>
        {/* {head.title} */}
        {loc.url.pathname === "/"
          ? "Nestri – Your games. Your rules.":
          loc.url.pathname.startsWith("/moq/")
            ?
            `MoQ – Nestri`
            : `${loc.url.pathname.split("/")[1].charAt(0).toUpperCase() + loc.url.pathname.split("/")[1].slice(1)} – Nestri`
        }
      </title>

      <link rel="canonical" href={loc.url.href} />
      <meta name="viewport" content="width=device-width, initial-scale=1.0" />
      {/**For SEO optimisation purposes refrain from SVG favicons and use PNG instead */}
      <link rel="apple-touch-icon" sizes="180x180" href="/seo/apple-touch-icon.png" />
      <link rel="icon" type="image/png" sizes="32x32" href="/seo/favicon-32x32.png" />
      <link rel="icon" type="image/png" sizes="16x16" href="/seo/favicon-16x16.png" />
      <link rel="manifest" href="/manifest.json" />
      {/**@ts-ignore */}
      <link rel="mask-icon" href="/seo/safari-pinned-tab.svg" color="#ffede5" />
      <link rel="shortcut icon" href="/seo/favicon.ico" />
      <meta name="msapplication-TileColor" content="#ffede5" />
      <meta name="msapplication-config" content="/seo/browserconfig.xml" />
      <meta property="og:url" content={domain} />
      <meta property="twitter:url" content={domain} />
      <meta property="twitter:domain" content={domain.replace("https://", "")} />
      <meta property="og:type" content="website" />
      {!loc.url.pathname.startsWith("/blog/") && (
        <>
          <meta property="og:image" content={`${domain}/seo/banner.png`} />
          <meta property="twitter:image" content={`${domain}/seo/banner.png`} />
        </>
      )}
      {
        head.meta.map((m) => (
          <meta key={m.key} {...m} />
        ))
      }

      {
        head.links.map((l) => (
          <link key={l.key} {...l} />
        ))
      }

      {
        head.styles.map((s) => (
          <style
            key={s.key}
            {...s.props}
            {...(s.props?.dangerouslySetInnerHTML
              ? {}
              : { dangerouslySetInnerHTML: s.style })}
          />
        ))
      }

      {
        head.scripts.map((s) => (
          <script
            key={s.key}
            {...s.props}
            {...(s.props?.dangerouslySetInnerHTML
              ? {}
              : { dangerouslySetInnerHTML: s.script })}
          />
        ))
      }
    </>
  );
});
