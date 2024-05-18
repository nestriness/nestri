<div align="center">
<div>

<a href="https://netris.me" >
<img height="160" src="/assets/logo.png">
</a>

</div>

&nbsp;
&nbsp;

<div align="center" >
<h1>
<a href="https://netris.me" >
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="/assets/logo-name-white.png">
  <source media="(prefers-color-scheme: light)" srcset="/assets/logo-name-black.png">
  <img alt="netris logo name" src="/assets/logo-name-black.png">
</picture>

</a>

</h1>

</div>
&nbsp;
&nbsp;

An open-source cloud gaming platform built for you to play together with your friends. <br/> A GeForce NOW alternative that can be self-hosted.<br/>

</div>

&nbsp;
&nbsp;
 <div align="center" >

[![][github-release-shield]][github-release-link]
[![][discord-shield]][discord-link]
[![][github-license-shield]][github-license-link]
[![][github-stars-shield]][github-stars-link]

**Share the Netris Repository on Social Media**

[![][share-x-shield]][share-x-link]
[![][share-reddit-shield]][share-reddit-link]

</div>
&nbsp;
&nbsp;

<div align="center" >

[![][image-overview]][website-link]

</div>

## Getting Started

Before you begin, ensure you meet the following prerequisites to use Netris on your system.

### Prerequisites

- **Nvidia GPU**: Unfortunately, this setup is exclusive to Nvidia GPUs. If you don't own one, consider renting from cloud services like AWS, GCP, or Vast.ai. We highly recommend this approach.

- **CUDA**: For GPU acceleration, CUDA version `12.0` or newer is required. Verify your CUDA installation by running `nvcc --version`.

- **Docker**: Ensure you have `docker` and `nvidia-docker` are up to date to avoid compatibility issues with CUDA. You can check your Docker version by running `docker --version` in your terminal.

- **GPU Driver**: Ensure your GPU drivers are up to date to avoid compatibility issues with CUDA. Nvidia driver version `520.56.06` or newer is required.

- **Xorg Display**: Your Nvidia GPU should not be attached to a running X display server. You can confirm this by running `nvidia-smi`.

- **Nvidia-DRM**: Make sure that the `nvidia-drm` module has been loaded and that the module is loaded with the flag `modeset=1`. Confirm this by running `sudo cat /sys/module/nvidia_drm/parameters/modeset` 

> \[!TIP]
>
> Typically, if your setup meets the necessary CUDA requirements, the `nvidia-drm` module will already be loaded, particularly in AWS G4dn instances. \~ 💡

> \[!IMPORTANT]
>
> We need all this prerequisites met for hardware accelerated video encoding and gamescope to work to work. \~ 📽️✨


[![][image-star]][github-stars-link]

[github-release-link]: https://github.com/wanjohiryan/netris/releases
[github-release-shield]: https://img.shields.io/github/v/release/wanjohiryan/netris?color=369eff&labelColor=black&logo=github&style=flat-square
[discord-shield]: https://img.shields.io/discord/1080111004698021909?color=5865F2&label=discord&labelColor=black&logo=discord&logoColor=white&style=flat-square
[discord-link]: https://discord.com/invite/Y6etn3qKZ3
[github-license-shield]: https://img.shields.io/github/license/wanjohiryan/netris?color=white&labelColor=black&style=flat-square
[github-license-link]: https://github.com/wanjohiryan/netris/blob/main/LICENSE
[github-stars-shield]: https://img.shields.io/github/stars/wanjohiryan/netris?color=ffcb47&labelColor=black&style=flat-square
[github-stars-link]: https://github.com/wanjohiryan/netris/network/stargazers
[share-x-shield]: https://img.shields.io/badge/-share%20on%20x-black?labelColor=black&logo=x&logoColor=white&style=flat-square
[share-x-link]: https://twitter.com/intent/tweet?text=Hey%2C%20check%20out%20this%20Github%20repository.%20It%20is%20an%20open-source%20self-hosted%20Geforce%20Now%20alternative.&url=https%3A%2F%2Fgithub.com%2Fwanjohiryan%2Fnetris
[share-reddit-shield]: https://img.shields.io/badge/-share%20on%20reddit-black?labelColor=black&logo=reddit&logoColor=white&style=flat-square
[share-reddit-link]: https://www.reddit.com/submit?title=Hey%2C%20check%20out%20this%20Github%20repository.%20It%20is%20an%20open-source%20self-hosted%20Geforce%20Now%20alternative.&url=https%3A%2F%2Fgithub.com%2Fwanjohiryan%2Fnetris
[image-overview]: assets/banner.png
[website-link]: https://netris.me
[image-star]: assets/star-us.png