import { component$ } from "@builder.io/qwik"
import { NavBar, Footer } from "@nestri/ui";
import { buttonVariants, cn } from "@nestri/ui/design";
import { MotionComponent, transition, TitleSection } from "@nestri/ui/react";


const feedback = [
    {
        rating: 5,
        icon: () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32" fill="none">
                <path d="M15.9989 29.9978C25.3333 29.9978 29.9978 23.7303 29.9978 15.9989C29.9978 8.26751 25.3333 2 15.9989 2C6.66443 2 2 8.26751 2 15.9989C2 23.7303 6.66443 29.9978 15.9989 29.9978Z" fill="#FFB02E" />
                <path d="M16 25C7 25 7 16 7 16H25C25 16 25 25 16 25Z" fill="#BB1D80" />
                <path d="M8 16.5V16H24V16.5C24 17.0523 23.5523 17.5 23 17.5H9C8.44772 17.5 8 17.0523 8 16.5Z" fill="white" />
                <path d="M3.18104 9.75037C5.19703 12.0771 7.8791 13.096 9.25386 13.4894C9.81699 13.6506 10.4079 13.4889 10.8249 13.0776C12.0184 11.9005 14.4238 9.19933 14.938 6.11531C15.656 1.80872 10.256 0.495856 8.07985 4.04542C2.98933 1.65437 0.296489 6.42127 3.18104 9.75037Z" fill="#F70A8D" />
                <path d="M28.8172 9.75198C26.8022 12.0775 24.1215 13.0961 22.7473 13.4894C22.1841 13.6506 21.5932 13.4889 21.1762 13.0776C19.9831 11.9008 17.579 9.20094 17.0651 6.11839C16.3474 1.81356 21.7452 0.50123 23.9204 4.04935C29.0089 1.65928 31.7006 6.42423 28.8172 9.75198Z" fill="#F70A8D" />
            </svg>
        )
    },
    {
        rating: 4,
        icon: () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32" fill="none">
                <path d="M15.9989 29.9978C25.3333 29.9978 29.9978 23.7303 29.9978 15.9989C29.9978 8.26751 25.3333 2 15.9989 2C6.66443 2 2 8.26751 2 15.9989C2 23.7303 6.66443 29.9978 15.9989 29.9978Z" fill="#FFB02E" />
                <path d="M11.8395 18.4566C11.5433 17.9987 10.9333 17.8621 10.4695 18.1523C10.0013 18.4453 9.85933 19.0624 10.1523 19.5305L10.1531 19.5317L10.1538 19.5328L10.1554 19.5354L10.1589 19.5409L10.1676 19.5543C10.1741 19.5642 10.1819 19.5759 10.1913 19.5895C10.2099 19.6165 10.2343 19.6507 10.2648 19.6909C10.3259 19.7713 10.4117 19.8761 10.5249 19.9967C10.7514 20.2382 11.0879 20.5432 11.5554 20.8423C12.4995 21.4464 13.9369 22 16 22C18.0632 22 19.5005 21.4464 20.4446 20.8423C20.9121 20.5432 21.2486 20.2382 21.4752 19.9967C21.5883 19.8761 21.6741 19.7713 21.7352 19.6909C21.7657 19.6507 21.7902 19.6165 21.8088 19.5895C21.8181 19.5759 21.826 19.5642 21.8324 19.5543L21.8411 19.5409L21.8447 19.5354L21.8462 19.5328L21.847 19.5317L21.8477 19.5305C22.1409 19.0625 21.9987 18.4453 21.5305 18.1523C21.0667 17.8621 20.4567 17.9987 20.1606 18.4566C20.1582 18.4599 20.1523 18.4683 20.1426 18.481C20.1206 18.51 20.0793 18.5614 20.0166 18.6283C19.8913 18.7619 19.6806 18.9568 19.3667 19.1577C18.7478 19.5536 17.6852 20 16 20C14.3148 20 13.2522 19.5536 12.6333 19.1577C12.3194 18.9568 12.1088 18.7619 11.9834 18.6283C11.9207 18.5614 11.8794 18.51 11.8574 18.481C11.8477 18.4683 11.8418 18.4599 11.8395 18.4566Z" fill="#402A32" />
                <path d="M11 5H5C2.79086 5 1 6.79086 1 9V9.67376C1 12.3252 2.49802 14.749 4.8695 15.9348L5.80534 16.4027C7.20729 17.1036 8.84913 17.1967 10.3252 16.6696C13.1112 15.6746 15 13.0253 15 10.067V9C15 6.79086 13.2091 5 11 5Z" fill="#321B41" />
                <path d="M20.5 5H27.5C29.433 5 31 6.567 31 8.5V9.67376C31 12.3252 29.502 14.749 27.1305 15.9348L26.1947 16.4027C24.7927 17.1036 23.1509 17.1967 21.6748 16.6696C18.8888 15.6746 17 13.0253 17 10.067V8.5C17 6.567 18.567 5 20.5 5Z" fill="#321B41" />
                <path d="M14 6.35418C13.2671 5.52375 12.1947 5 11 5H5C2.79086 5 1 6.79086 1 9V9.67376C1 12.3252 2.49802 14.749 4.8695 15.9348L5.80534 16.4027C7.20729 17.1036 8.84912 17.1967 10.3252 16.6696C13.1112 15.6746 15 13.0253 15 10.067V10C15 9.44772 15.4477 9 16 9C16.5523 9 17 9.44772 17 10V10.067C17 13.0253 18.8888 15.6746 21.6748 16.6696C23.1509 17.1967 24.7927 17.1036 26.1947 16.4027L27.1305 15.9348C29.502 14.749 31 12.3252 31 9.67376V8.5C31 6.567 29.433 5 27.5 5H20.5C19.3759 5 18.3756 5.52992 17.7352 6.35357V6.35077C17.7206 6.36537 17.704 6.3824 17.6855 6.40132C17.4936 6.59759 17.1031 6.9969 16.7395 7H14.9957C14.6321 6.9969 14.2418 6.59771 14.0499 6.40145C14.0314 6.38253 14.0146 6.36538 14 6.35077V6.35418ZM11 6C12.6569 6 14 7.34315 14 9V10.067C14 12.5975 12.3817 14.8732 9.9889 15.7278C8.76683 16.1643 7.40816 16.086 6.25255 15.5082L5.31672 15.0403C3.28401 14.024 2 11.9464 2 9.67376V9C2 7.34315 3.34315 6 5 6H11ZM18 8.5C18 7.11929 19.1193 6 20.5 6H27.5C28.8807 6 30 7.11929 30 8.5V9.67376C30 11.9464 28.716 14.024 26.6833 15.0403L25.7474 15.5082C24.5918 16.086 23.2332 16.1643 22.0111 15.7278C19.6183 14.8732 18 12.5975 18 10.067V8.5Z" fill="#8D65C5" />
                <path d="M12.3891 10.2678C12.9749 9.68199 12.8166 8.57395 12.0356 7.7929C11.2545 7.01186 10.1465 6.85356 9.56069 7.43935C8.9749 8.02514 9.13319 9.13318 9.91424 9.91422C10.6953 10.6953 11.8033 10.8536 12.3891 10.2678Z" fill="white" />
                <path d="M28.3891 10.2678C28.9749 9.68199 28.8166 8.57395 28.0356 7.7929C27.2545 7.01186 26.1465 6.85356 25.5607 7.43935C24.9749 8.02514 25.1332 9.13318 25.9142 9.91422C26.6953 10.6953 27.8033 10.8536 28.3891 10.2678Z" fill="white" />
            </svg>
        )
    },
    {
        rating: 3,
        icon: () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32" fill="none">
                <path d="M15.9989 29.9978C25.3333 29.9978 29.9978 23.7303 29.9978 15.9989C29.9978 8.26751 25.3333 2 15.9989 2C6.66443 2 2 8.26751 2 15.9989C2 23.7303 6.66443 29.9978 15.9989 29.9978Z" fill="#FFB02E" />
                <path d="M6 13.5C6 13.331 6.00932 13.1642 6.02746 13H10.0313L12.332 13.9227L14.4639 13H14.9725C14.9907 13.1642 15 13.331 15 13.5C15 15.9853 12.9853 18 10.5 18C8.01472 18 6 15.9853 6 13.5Z" fill="white" />
                <path d="M17 13.5C17 13.331 17.0093 13.1642 17.0275 13H21.0407L23.2816 13.7124L25.448 13H25.9725C25.9907 13.1642 26 13.331 26 13.5C26 15.9853 23.9853 18 21.5 18C19.0147 18 17 15.9853 17 13.5Z" fill="white" />
                <path d="M10 13.25C10 13.1655 10.0046 13.0821 10.0137 13H14.4863C14.4954 13.0821 14.5 13.1655 14.5 13.25C14.5 14.4945 13.4945 15.5 12.25 15.5C11.0055 15.49 10 14.4845 10 13.25Z" fill="#402A32" />
                <path d="M21 13.25C21 13.1655 21.0046 13.0821 21.0137 13H25.4863C25.4954 13.0821 25.5 13.1655 25.5 13.25C25.5 14.4945 24.4945 15.5 23.25 15.5C22.0055 15.49 21 14.4845 21 13.25Z" fill="#402A32" />
                <path d="M8.06915 7.98761C7.47625 8.55049 7.11769 9.22774 6.97423 9.65811C6.88691 9.92009 6.60375 10.0617 6.34178 9.97434C6.07981 9.88702 5.93823 9.60386 6.02555 9.34189C6.21542 8.77226 6.65687 7.94951 7.38064 7.26239C8.1129 6.5672 9.1478 6 10.4999 6C10.776 6 10.9999 6.22386 10.9999 6.5C10.9999 6.77614 10.776 7 10.4999 7C9.45198 7 8.65355 7.4328 8.06915 7.98761Z" fill="#402A32" />
                <path d="M23.9309 7.98761C24.5238 8.55049 24.8823 9.22774 25.0258 9.65811C25.1131 9.92009 25.3963 10.0617 25.6582 9.97434C25.9202 9.88702 26.0618 9.60386 25.9745 9.34189C25.7846 8.77226 25.3431 7.94951 24.6194 7.26239C23.8871 6.5672 22.8522 6 21.5001 6C21.224 6 21.0001 6.22386 21.0001 6.5C21.0001 6.77614 21.224 7 21.5001 7C22.548 7 23.3465 7.4328 23.9309 7.98761Z" fill="#402A32" />
                <path d="M23.9466 21.2622C24.1246 20.7393 23.845 20.1713 23.3222 19.9933C22.7993 19.8153 22.2313 20.0949 22.0533 20.6178C21.1017 23.4135 18.0618 24.9046 15.2647 23.9442C14.7424 23.7648 14.1735 24.0429 13.9942 24.5652C13.8148 25.0876 14.0929 25.6564 14.6152 25.8358C18.4581 27.1553 22.6382 25.1065 23.9466 21.2622Z" fill="#402A32" />
            </svg>
        )
    }, {
        rating: 2,
        icon: () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32" fill="none">
                <path d="M15.9989 29.9978C25.3333 29.9978 29.9978 23.7303 29.9978 15.9989C29.9978 8.26751 25.3333 2 15.9989 2C6.66443 2 2 8.26751 2 15.9989C2 23.7303 6.66443 29.9978 15.9989 29.9978Z" fill="#FFB02E" />
                <path d="M10.4191 16.2244C12.742 16.2244 14.6251 14.3414 14.6251 12.0185C14.6251 9.69557 12.742 7.8125 10.4191 7.8125C8.09621 7.8125 6.21313 9.69557 6.21313 12.0185C6.21313 14.3414 8.09621 16.2244 10.4191 16.2244Z" fill="white" />
                <path d="M21.5683 16.3011C23.9123 16.3011 25.8126 14.4009 25.8126 12.0568C25.8126 9.71274 23.9123 7.8125 21.5683 7.8125C19.2242 7.8125 17.324 9.71274 17.324 12.0568C17.324 14.4009 19.2242 16.3011 21.5683 16.3011Z" fill="white" />
                <path d="M11 15C12.6569 15 14 13.6569 14 12C14 10.3431 12.6569 9 11 9C9.34315 9 8 10.3431 8 12C8 13.6569 9.34315 15 11 15Z" fill="#402A32" />
                <path d="M21 15C22.6569 15 24 13.6569 24 12C24 10.3431 22.6569 9 21 9C19.3431 9 18 10.3431 18 12C18 13.6569 19.3431 15 21 15Z" fill="#402A32" />
                <path d="M10 20C10 19.4477 10.4477 19 11 19H21C21.5523 19 22 19.4477 22 20C22 20.5523 21.5523 21 21 21H11C10.4477 21 10 20.5523 10 20Z" fill="#402A32" />
            </svg>
        )
    }, {
        rating: 1,
        icon: () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 32 32" fill="none">
                <path d="M15.9989 29.9978C25.3333 29.9978 29.9978 23.7303 29.9978 15.9989C29.9978 8.26751 25.3333 2 15.9989 2C6.66443 2 2 8.26751 2 15.9989C2 23.7303 6.66443 29.9978 15.9989 29.9978Z" fill="#F8312F" />
                <path d="M10.5 19C12.9853 19 15 16.9853 15 14.5C15 12.0147 12.9853 10 10.5 10C8.01472 10 6 12.0147 6 14.5C6 16.9853 8.01472 19 10.5 19Z" fill="white" />
                <path d="M21.5 19C23.9853 19 26 16.9853 26 14.5C26 12.0147 23.9853 10 21.5 10C19.0147 10 17 12.0147 17 14.5C17 16.9853 19.0147 19 21.5 19Z" fill="white" />
                <path d="M14.9989 11.2899C15.0209 10.8763 14.7035 10.5231 14.2899 10.5011C13.4607 10.4569 12.7846 10.2597 12.2504 9.88776C11.7235 9.52078 11.2715 8.93987 10.9612 8.01214C10.8299 7.61931 10.4049 7.40737 10.0121 7.53875C9.61925 7.67013 9.40731 8.09508 9.53869 8.48791C9.93308 9.66714 10.558 10.537 11.3932 11.1187C12.2213 11.6954 13.1929 11.9447 14.21 11.999C14.6237 12.021 14.9768 11.7036 14.9989 11.2899Z" fill="#402A32" />
                <path d="M17.001 11.2899C16.979 10.8763 17.2964 10.5231 17.71 10.5011C18.5392 10.4569 19.2153 10.2597 19.7495 9.88776C20.2764 9.52078 20.7284 8.93987 21.0387 8.01214C21.1701 7.61931 21.595 7.40737 21.9878 7.53875C22.3807 7.67013 22.5926 8.09508 22.4612 8.48791C22.0668 9.66714 21.442 10.537 20.6067 11.1187C19.7786 11.6954 18.807 11.9447 17.7899 11.999C17.3763 12.021 17.0231 11.7036 17.001 11.2899Z" fill="#402A32" />
                <path d="M14 15C14 16.1046 13.1046 17 12 17C10.8954 17 10 16.1046 10 15C10 13.8954 10.8954 13 12 13C13.1046 13 14 13.8954 14 15Z" fill="#402A32" />
                <path d="M22 15C22 16.1046 21.1046 17 20 17C18.8954 17 18 16.1046 18 15C18 13.8954 18.8954 13 20 13C21.1046 13 22 13.8954 22 15Z" fill="#402A32" />
                <path d="M7 21.5C7 20.6716 7.67157 20 8.5 20H23.5C24.3284 20 25 20.6716 25 21.5V27.5C25 28.3284 24.3284 29 23.5 29H8.5C7.67157 29 7 28.3284 7 27.5V21.5Z" fill="#533566" />
                <path d="M12.1756 22.0318C12.4341 22.1288 12.5651 22.417 12.4682 22.6756L12.3465 23H13.7785L14.0318 22.3245C14.1288 22.0659 14.417 21.9349 14.6756 22.0318C14.9341 22.1288 15.0651 22.417 14.9682 22.6756L14.8465 23H15C15.2761 23 15.5 23.2239 15.5 23.5C15.5 23.7762 15.2761 24 15 24H14.4715L14.0965 25H14.5C14.7761 25 15 25.2239 15 25.5C15 25.7762 14.7761 26 14.5 26H13.7215L13.4682 26.6756C13.3712 26.9341 13.083 27.0651 12.8244 26.9682C12.5659 26.8712 12.4349 26.583 12.5318 26.3245L12.6535 26H11.2215L10.9682 26.6756C10.8712 26.9341 10.583 27.0651 10.3244 26.9682C10.0659 26.8712 9.93488 26.583 10.0318 26.3245L10.1535 26H10C9.72386 26 9.5 25.7762 9.5 25.5C9.5 25.2239 9.72386 25 10 25H10.5285L10.9035 24H10.5C10.2239 24 10 23.7762 10 23.5C10 23.2239 10.2239 23 10.5 23H11.2785L11.5318 22.3245C11.6288 22.0659 11.917 21.9349 12.1756 22.0318ZM13.4035 24H11.9715L11.5965 25H13.0285L13.4035 24Z" fill="white" />
                <path d="M18.9682 22.6756C19.0651 22.417 18.9341 22.1288 18.6756 22.0318C18.417 21.9349 18.1288 22.0659 18.0318 22.3245L16.5318 26.3245C16.4349 26.583 16.5659 26.8712 16.8244 26.9682C17.083 27.0651 17.3712 26.9341 17.4682 26.6756L18.9682 22.6756Z" fill="white" />
                <path d="M19 27C19.2761 27 19.5 26.7762 19.5 26.5C19.5 26.2239 19.2761 26 19 26C18.7239 26 18.5 26.2239 18.5 26.5C18.5 26.7762 18.7239 27 19 27Z" fill="white" />
                <path d="M22 22.5C22 22.2239 21.7761 22 21.5 22C21.2239 22 21 22.2239 21 22.5V24.5C21 24.7762 21.2239 25 21.5 25C21.7761 25 22 24.7762 22 24.5V22.5Z" fill="white" />
                <path d="M16.5 23C16.7761 23 17 22.7762 17 22.5C17 22.2239 16.7761 22 16.5 22C16.2239 22 16 22.2239 16 22.5C16 22.7762 16.2239 23 16.5 23Z" fill="white" />
                <path d="M22 26.5C22 26.7762 21.7761 27 21.5 27C21.2239 27 21 26.7762 21 26.5C21 26.2239 21.2239 26 21.5 26C21.7761 26 22 26.2239 22 26.5Z" fill="white" />
            </svg>
        )
    }
]

export default component$(() => {


    return (
        <>
            <NavBar />
            <TitleSection client:load title="Contact" description="Need help? Found a bug? Have a suggestion? Let us know!" />

            <MotionComponent
                initial={{ opacity: 0, y: 100 }}
                whileInView={{ opacity: 1, y: 0 }}
                viewport={{ once: true }}
                transition={transition}
                client:load
                class="flex items-center justify-center w-full"
                as="div"
            >
                <section class="w-full flex justify-center items-center">
                    <div class="w-full max-w-xl space-y-6 px-3">
                        <div class="flex gap-3 w-full justify-around" >
                            {feedback.map((item, index) => (
                                <div key={`emoji-${index}`} class="flex relative">
                                    <input type="radio" class="hidden peer" name="feedback-emoji" value={item.rating} id={`emoji-${index + 1}`} />
                                    <label for={`emoji-${index + 1}`} class="peer-checked:bg-gray-300/70 dark:peer-checked:bg-gray-700/70 border-gray-300 dark:border-gray-700 peer-checked:ring-gray-400 dark:peer-checked:ring-gray-600 peer-checked:ring-offset-gray-50 dark:peer-checked:ring-offset-gray-950 peer-checked:ring-2 peer-checked:ring-offset-2 border cursor-pointer bg-gray-200/70 dark:bg-gray-800/70 rounded-full p-3">
                                        <item.icon />
                                    </label>
                                </div>
                            ))}
                        </div>
                        <div class="bg-gray-200/70 dark:bg-gray-800/70 flex rounded-lg w-full relative h-10 flex-none border focus-within:bg-gray-300/50 dark:focus-within:bg-gray-700/50 border-gray-300 dark:border-gray-700 ">
                            <input type="text" class="w-full h-full bg-transparent rounded-lg p-3 focus-within:outline-none focus-within:ring-2 focus-within:ring-gray-400 dark:focus-within:ring-gray-600 focus-within:ring-offset-2 focus-visible:outline-none focus-within:ring-offset-gray-50 dark:focus-within:ring-offset-gray-950 placeholder:text-gray-500/70" placeholder="Full Name" />
                        </div>
                        <div class="bg-gray-200/70 dark:bg-gray-800/70 flex rounded-lg w-full relative h-10 flex-none border focus-within:bg-gray-300/50 dark:focus-within:bg-gray-700/50 border-gray-300 dark:border-gray-700 ">
                            <input type="email" class="w-full h-full bg-transparent rounded-lg p-3 focus-within:outline-none focus-within:ring-2 focus-within:ring-gray-400 dark:focus-within:ring-gray-600 focus-within:ring-offset-2 focus-visible:outline-none focus-within:ring-offset-gray-50 dark:focus-within:ring-offset-gray-950 placeholder:text-gray-500/70" placeholder="Email Address" />
                        </div>
                        <div class="bg-gray-200/70 dark:bg-gray-800/70 flex rounded-lg w-full relative h-max flex-none border focus-within:bg-gray-300/50 dark:focus-within:bg-gray-700/50 border-gray-300 dark:border-gray-700 ">
                            <textarea class="resize-y overflow-y-auto whitespace-break-spaces [form-sizing:content] min-h-[193px] w-full h-full bg-transparent rounded-lg p-3 focus-within:outline-none focus-within:ring-2 focus-within:ring-gray-400 dark:focus-within:ring-gray-600 focus-within:ring-offset-2 focus-visible:outline-none focus-within:ring-offset-gray-50 dark:focus-within:ring-offset-gray-950 placeholder:text-gray-500/70" placeholder="Your message" />
                        </div>
                        <button class={cn(buttonVariants.solid({ size: "md", intent: "neutral" }), "w-full")} type="submit" >
                            Submit
                        </button>
                    </div>
                </section>
            </MotionComponent>
            <Footer />
        </>
    )
})