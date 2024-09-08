import { component$, Slot } from "@builder.io/qwik";
// import { useContent, type RequestHandler } from "@builder.io/qwik-city";
import { Footer } from "@nestri/ui";

export default component$(() => {
    // useStyles$(styles)
    // const { frontmatter } = useDocumentHead()
    // console.log("frontmatter", frontmatter)

    return (
        <>
            <Slot />
            <Footer/>
        </>
    );
});