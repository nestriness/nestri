export const openWindow = (url: string): Promise<void> => {
    return new Promise((resolve) => {
        const isMobile = /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);

        if (isMobile) {
            // Open in a new tab for mobile devices
            const newTab = window.open(url, '_blank');
            localStorage.removeItem("auth_success")

            if (newTab) {
                const pollTimer = setInterval(() => {

                    if (newTab.closed) {
                        clearInterval(pollTimer);
                        resolve();  

                        return;
                    } else if (localStorage.getItem("auth_success")) {
                        clearInterval(pollTimer);
                        newTab.location.href = 'about:blank';

                        setTimeout(() => {
                            newTab.close();
                            window.focus();
                            //TODO: Navigate and start onboarding
                        }, 50);

                        localStorage.removeItem("auth_success")

                        resolve();
                        return;
                    }
                }, 300);
            } else {
                resolve(); // Resolve if popup couldn't be opened
            }

        } else {
            // Open in a popup for desktop devices
            const width = 600;
            const height = 600;
            const top = window.top!.outerHeight / 2 + window.top!.screenY - (height / 2);
            const left = window.top!.outerWidth / 2 + window.top!.screenX - (width / 2);

            const popup = window.open(
                url,
                'Nestri Auth Popup',
                `width=${width},height=${height},left=${left},top=${top},toolbar=no, location=no, directories=no, status=no, menubar=no, scrollbars=no, resizable=no, copyhistory=no,`
            );

            if (popup) {
                const pollTimer = setInterval(() => {

                    if (popup.closed) {
                        clearInterval(pollTimer);
                        localStorage.removeItem("auth_success")
                        resolve();
                        return;
                    }

                }, 300);
            } else {
                resolve(); // Resolve if popup couldn't be opened
            }
        }
    });
};