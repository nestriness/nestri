//Copied from https://github.com/qwikifiers/qwik-ui/blob/main/packages/kit-headless/src/components/modal/index.ts
//Why? because qwik-ui/headless requires qwik v1.7.2 as a peer dependency,  which is causing build errors in the monorepo
// Reference:    https://github.com/qwikifiers/qwik-ui/blob/26c17886e9a84de9d0da09f1180ede5fdceb70f3/packages/kit-headless/package.json#L33

export { HModalRoot as Root } from './modal-root';
export { HModalPanel as Panel } from './modal-panel';
export { HModalContent as Content } from './modal-content';
export { HModalFooter as Footer } from './modal-footer';
export { HModalHeader as Header } from './modal-header';
export { HModalTitle as Title } from './modal-title';
export { HModalDescription as Description } from './modal-description';
export { HModalTrigger as Trigger } from './modal-trigger';
export { HModalClose as Close } from './modal-close';
