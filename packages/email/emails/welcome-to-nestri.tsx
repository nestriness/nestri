// Sent after someone signs up for the waitlist
// Sent to someone who is already on the waitlist and we need to invite to the app// Sent when an adult invites someone to a team as a child or an adult

import {
    Body,
    Container,
    Heading,
    Hr,
    Html,
    Img,
    Link,
    Preview,
    Section,
    Tailwind,
    Text,
    Font,
    Button
} from "@react-email/components";

const baseUrl =
    process.env.VERCEL_ENV === "production"
        ? "https://nestri.pages.dev/email"
        : "http://localhost:5173/email";

export default function WelcomeToNestri({
    name = "Ryan",
    link = "https://nestri.io/join/djnslnposn"
}: {
    name: string;
    role: "adult" | "child";
    login: boolean,
    sender: string,
    team: string,
    link: string,
    senderEmail: string
}) {
    return (
        <Html lang="en" className="p-0 m-0 w-full">
            <Tailwind
                config={{
                    theme: {
                        extend: {
                            colors: {
                                text: {
                                    DEFAULT: "#0a0a0a",
                                    dark: "#fafafa",
                                    muted: "#fafafab3",
                                    mutedDark: "#0a0a0ab3",
                                },
                                bg: {
                                    DEFAULT: "#f5f5f5",
                                    dark: "#171717",
                                },
                                link: {
                                    DEFAULT: "#FF7033",
                                    dark: "#CC3D00",
                                },
                                border: {
                                    DEFAULT: "#d4d4d4",
                                    dark: "#404040",
                                }
                            },
                            fontFamily: {
                                'sans': ['Geist Sans', 'sans-serif'],
                                'title': ['Bricolage Grotesque', 'sans-serif'],
                            }
                        },
                    },
                }}
            >
                <head>
                    <Font
                        fontFamily="Geist"
                        fallbackFontFamily="Helvetica"
                        webFont={{
                            url: "https://cdn.jsdelivr.net/npm/@fontsource/geist-sans@5.0.1/files/geist-sans-latin-400-normal.woff2",
                            format: "woff2",
                        }}
                        fontWeight={400}
                        fontStyle="normal"
                    />

                    <Font
                        fontFamily="Bricolage Grotesque"
                        fallbackFontFamily="Helvetica"
                        webFont={{
                            url: "https://cdn.jsdelivr.net/fontsource/fonts/bricolage-grotesque@latest/latin-700-normal.woff2",
                            format: "woff2",
                        }}
                        fontWeight={700}
                        fontStyle="normal"
                    />

                    <Font
                        fontFamily="Geist"
                        fallbackFontFamily="Helvetica"
                        webFont={{
                            url: "https://cdn.jsdelivr.net/npm/@fontsource/geist-sans@5.0.1/files/geist-sans-latin-500-normal.woff2",
                            format: "woff2",
                        }}
                        fontWeight={500}
                        fontStyle="normal"
                    />
                </head>
                <Preview>You've been invited to join early access</Preview>
                <Body className="py-8 sm:px-0 px-2 bg-[#FFF] dark:bg-[#000] text-text dark:text-text-dark font-sans">
                    <Container className="max-w-[28rem] mx-auto">
                        <Link href="https://nestri.io">
                            <Img
                                width={64}
                                className="block outline-none border-none text-decoration-none h-auto"
                                src={`${baseUrl}/logo.webp`}
                                alt="Nestri logo" />
                        </Link>
                        <Heading className="m-0 text-text dark:text-text-dark font-title font-bold mt-8 text-2xl leading-5" >Hello&nbsp;{name}&nbsp;👋🏾</Heading>
                        <Hr className="my-4" />
                        <Text className="text-base">
                            Welcome to Nestri!
                        </Text>
                        <Text className="text-base">
                            My name is Wanjohi, I am the founder of Nestri
                        </Text>
                        <Text className="text-base">
                            I saw you recently got early access to Nestri and I wanted to reach out and say hi!
                        </Text>
                        <Text className="text-base">
                            We've been working on Nestri for a few months now, and we've completed the basic functionality required to get started.
                        </Text>
                        <Text className="text-base">
                            During our <strong>Alpha</strong> phase, you may encounter bugs or find that some features you want are missing - and we would
                            genuinely want to hear your feedback
                        </Text>
                        <Text className="text-base">
                            Should you have any questions or comments, please don't hesitate to reply directly to this email or&nbsp;<Link style={{ textDecoration: "underline" }} href="https://cal.com/wanjohi/15min" className="underline underline-offset-2 text-[#0a0a0ab3] dark:text-[#fafafab3]">
                            schedule a 15 minute call with me
                            </Link>
                        </Text>
                        <Text className="text-base">
                            Thank you for your support and am hoping to hear all about your Nestri experience soon
                        </Text>
                        <Text className="text-base">
                            Bravo Six Going Dark,
                            <br/>
                            <span className="line-through">Wanjohi Ryan</span>
                        </Text>
                        <Hr className="my-2" />
                        <Section>
                            <Heading style={{ float: "left" }} className="text-text dark:text-text-dark text-base font-normal">
                                <Link className="text-text dark:text-text-dark" href="https://nestri.io">Nestri</Link>
                            </Heading>
                            <Heading style={{ float: "right" }} className="text-text dark:text-text-dark text-base font-normal">
                                Your Games. Your Rules.
                            </Heading>
                        </Section>
                    </Container>
                </Body>
            </Tailwind>
        </Html >
    )
}