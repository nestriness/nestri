import { component$, useStyles$, useVisibleTask$ } from '@builder.io/qwik'
import { useLocation } from '@builder.io/qwik-city'
import type { NProgressOptions } from 'nprogress'
import nProgress from 'nprogress'
import nProgressStyles from 'nprogress/nprogress.css?inline'
import nPrStyles from './npr.css?inline'

interface Props {
    options?: Partial<NProgressOptions> &
    Partial<{
        color: string
        height: string
    }>
}

export const NavProgress = component$(({ options = {} }: Props) => {
    // const CSS_VAR_PREFIX = '--nav-progress-'

    useStyles$(nProgressStyles + nPrStyles)

    nProgress.configure({ showSpinner: false, ...options })

    const location = useLocation()

    // eslint-disable-next-line qwik/no-use-visible-task
    useVisibleTask$(({ track }) => {
        const isNavigating = track(() => location.isNavigating)
        if (isNavigating) nProgress.start()
        else nProgress.done()
    })

    // return (
    //     <style
    //         dangerouslySetInnerHTML={`
    //   :root {
    //     ${options.color ? `${CSS_VAR_PREFIX}color: ${options.color};` : ''}
    //     ${options.height ? `${CSS_VAR_PREFIX}height: ${options.height};` : ''}
    //   }
    // `}
    //     ></style>
    // )
    return <div data-name="progress" />
})