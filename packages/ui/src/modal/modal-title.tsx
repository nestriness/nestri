import { type PropsOf, Slot, component$, useContext } from '@builder.io/qwik';
import { modalContextId } from './modal-context';

export type ModalTitleProps = PropsOf<'h2'>;

export const HModalTitle = component$((props: ModalTitleProps) => {
  const context = useContext(modalContextId);

  const titleId = `${context.localId}-title`;

  return (
    <h2 id={titleId} {...props}>
      <Slot />
    </h2>
  );
});
