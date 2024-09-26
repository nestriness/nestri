import { component$, $, useSignal } from "@builder.io/qwik";
import { TitleSection, Button } from "@nestri/ui/react";
import { cn } from "@nestri/ui/design";
import { Broadcast } from "./tester";
import { routeLoader$ } from "@builder.io/qwik-city";
import {
    type InitialValues,
    type SubmitHandler,
    useForm,
    valiForm$
} from "@modular-forms/qwik"
import * as v from "valibot"

const Schema = v.object({
    url: v.pipe(
        v.string(),
        v.minLength(10, "Please input a valid url"),
        v.url("Please input a valid url"),
    )
}, "Please fill in all the fields correctly.")

type Form = v.InferInput<typeof Schema>;

export const useFormLoader = routeLoader$<InitialValues<Form>>(async () => {
    return {
        url: ""
    }
})

const generateRandomWord = (length: number) => {
    const characters = 'abcdefghijklmnopqrstuvwxyz';
    return Array.from({ length }, () => characters[Math.floor(Math.random() * characters.length)]).join('');
};

export default component$(() => {
    const broadcasterOk = useSignal<boolean | undefined>();
    const [state, { Form, Field }] = useForm<Form>({
        loader: useFormLoader(),
        validate: valiForm$(Schema)
    });

    const handleSubmit = $<SubmitHandler<Form>>(async (values) => {
        const randomNamespace = generateRandomWord(6);
        const sub = await Broadcast.init({ url: values.url, fingerprint: undefined, namespace: randomNamespace })

        setTimeout(() => {
            broadcasterOk.value = sub.isSubscribed()
        }, 1000);
    });

    return (
        <>
            <TitleSection client:load title="MoQ Checker" description="Test the connection to your Media-Over-Quic relay!" />
            <section class="w-full flex flex-col gap-4 justify-center items-center">
                <Form onSubmit$={handleSubmit} class="w-full max-w-xl flex px-3 gap-2">
                    <Field name="url">
                        {(field, props) => {
                            return (
                                <div class="w-full flex flex-col gap-2">
                                    <div class="bg-gray-200 dark:bg-gray-800 flex rounded-lg w-full relative h-10 flex-none border focus-within:bg-gray-300/70 dark:focus-within:bg-gray-700/70 border-gray-300 dark:border-gray-700 ">
                                        <input type="url" class={cn("w-full relative h-full bg-transparent rounded-lg p-3  focus-within:outline-none focus-within:ring-2 focus-within:ring-gray-400 dark:focus-within:ring-gray-600 focus-within:ring-offset-2 focus-visible:outline-none focus-within:ring-offset-gray-100 dark:focus-within:ring-offset-gray-900 placeholder:text-gray-500/70", typeof broadcasterOk.value !== "undefined" && broadcasterOk.value == true && "ring-2 ring-offset-2 ring-offset-gray-100 dark:ring-offset-gray-900 ring-green-500", typeof broadcasterOk.value !== "undefined" && (broadcasterOk.value == false) && "ring-2 ring-offset-2 ring-offset-gray-100 dark:ring-offset-gray-900 ring-red-500")} placeholder="https://relay.domain.com" {...props} />
                                    </div>
                                    {field.error && (<p class='text-[0.8rem] font-medium text-danger-600 dark:text-danger-500' >{field.error}</p>)}
                                </div>
                            )
                        }}
                    </Field>

                    {/* <button class={cn(buttonVariants.solid({ size: "md", intent: "neutral" }), "w-max space-y-0 relative")} style={{ height: 40, marginTop: 0 }} type="submit" >
                        Check
                    </button> */}
                    <Button.Root
                        disabled={state.submitting}
                        isLoading={state.submitting}
                        // setIsLoading={setIsLoading}
                        client:load
                        //@ts-ignore
                        type="submit"
                        style={{ height: 40, marginTop: 0 }}
                        intent="neutral"
                        size="md"
                        class="w-max space-y-0 relative">
                        {/* <Button.Icon
                            isLoading={isLoading.value}
                            client:load>
                            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 16 16">
                                <g fill="currentColor">
                                    <path d="M.329 10.333A8.01 8.01 0 0 0 7.99 16C12.414 16 16 12.418 16 8s-3.586-8-8.009-8A8.006 8.006 0 0 0 0 7.468l.003.006l4.304 1.769A2.2 2.2 0 0 1 5.62 8.88l1.96-2.844l-.001-.04a3.046 3.046 0 0 1 3.042-3.043a3.046 3.046 0 0 1 3.042 3.043a3.047 3.047 0 0 1-3.111 3.044l-2.804 2a2.223 2.223 0 0 1-3.075 2.11a2.22 2.22 0 0 1-1.312-1.568L.33 10.333Z" /><path d="M4.868 12.683a1.715 1.715 0 0 0 1.318-3.165a1.7 1.7 0 0 0-1.263-.02l1.023.424a1.261 1.261 0 1 1-.97 2.33l-.99-.41a1.7 1.7 0 0 0 .882.84Zm3.726-6.687a2.03 2.03 0 0 0 2.027 2.029a2.03 2.03 0 0 0 2.027-2.029a2.03 2.03 0 0 0-2.027-2.027a2.03 2.03 0 0 0-2.027 2.027m2.03-1.527a1.524 1.524 0 1 1-.002 3.048a1.524 1.524 0 0 1 .002-3.048" />
                                </g>
                            </svg>
                        </Button.Icon> */}
                        <Button.Label
                            loadingText="Checking..."
                            class="text-ellipsis whitespace-nowrap"
                            isLoading={state.submitting}>
                            Check
                        </Button.Label>
                        <div class="w-[8%]" />
                    </Button.Root>
                </Form>
                {typeof broadcasterOk.value !== "undefined" && broadcasterOk.value == true ? (
                    <span class="w-full text-green-500 max-w-xl flex space-y-6 px-3 gap-2">
                        Your relay is doing okay
                    </span>
                ) : typeof broadcasterOk.value !== "undefined" && (
                    <span class="w-full text-red-500 max-w-xl flex space-y-6 px-3 gap-2">
                        Your relay has an issue
                    </span>
                )}
            </section>
        </>
    )
})