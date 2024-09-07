import {
    Body,
    Container,
    Head,
    Heading,
    Hr,
    Html,
    Img,
    Link,
    Preview,
    Section,
    Tailwind,
    Text,
    Font
} from "@react-email/components";

const baseUrl =
    process.env.VERCEL_ENV === "production"
        ? "https://nestri.pages.dev/email"
        : "http://localhost:5173/email";

export default function Invite({
    name = "Wanjohi",
    inviteCode = "23476"
}: {
    name: string;
    inviteCode: string;
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
                <Preview>{name}, welcome to Nestri</Preview>
                <Body className="py-8 sm:px-0 px-2 bg-[#FFF] dark:bg-[#000] text-text dark:text-text-dark font-sans">
                    <Container className="max-w-[28rem] mx-auto">
                        <Link href="https://nestri.io">
                            <Img
                                width={64}
                                className="block outline-none border-none text-decoration-none h-auto"
                                src={`${baseUrl}/logo.webp`}
                                alt="Nestri logo" />
                        </Link>
                        <Heading className="m-0 text-text dark:text-text-dark font-title font-bold mt-8 text-2xl leading-5" >Hello&nbsp;{name}</Heading>
                        <Hr className="my-4" />
                        <Text className="text-base">
                            Welcome to Nestri!
                        </Text>
                        <Text className="text-base">
                            To verify your e-mail and activate your account, enter the following code.
                        </Text>
                        <Section className="overflow-hidden w-full">
                            <Text className="text-base py-2 px-4 rounded-lg border-[2px] border-solid border-border text-[#0a0a0ab3] dark:text-[#fafafab3] dark:border-border-dark w-max">
                                {inviteCode}
                            </Text>
                            <Text className="text-sm text-[#0a0a0ab3] dark:text-[#fafafab3]">
                                If you didn't request this, you can safely ignore this email.
                            </Text>
                        </Section>
                        <Text className="text-base">
                            Stay Frosty,
                            <br />
                            Team Nestri
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