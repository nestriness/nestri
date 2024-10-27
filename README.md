<div align="center">

<div align="center">
<h1>

<a href="https://nestri.io" >
<img src="/apps/www/public/seo/banner.png" alt="Nestri - What will you play next?">
</a>

</h1>
</div>

&nbsp;
&nbsp;

Nestri is an open-source, self-hosted Geforce Now alternative with Stadia's social features. <strong>Built and shaped by our gaming community.</strong>
<br/>
<br/>

</div>

<div align="center">

[![][github-release-shield]][github-release-link]
[![][discord-shield]][discord-link]
[![][github-license-shield]][github-license-link]
[![][github-stars-shield]][github-stars-link]

**Share the Nestri Repository on Social Media**

[![][share-x-shield]][share-x-link]
[![][share-reddit-shield]][share-reddit-link]

</div>
&nbsp;
&nbsp;

> **Note**
> Nestri is more closer (in feature comparison) to Jellyfin/Plex than Moonlight. Our goal is to develop a comprehensive self-hosted cloud gaming solution for your home server.

## Features

- Save and share your game progress easily with friends
- Simultaneously run multiple games on your GPU using Virtio-GPU Venus and/or Virgl
- Play games using either your integrated GPU or dedicated GPU
- Enjoy titles from your preferred Game Stores - Steam, Epic Games, Amazon Games, GOG.com
- Experience Android gaming
- Organize gaming sessions with friends and family through Nestri Parties
- Stream directly to YouTube and Twitch straight from your setup
- Family sharing capabilities
- Support for Controller, Touchscreen, Keyboard, and Mouse devices 

## Possible Use Cases

- Organize game nights or LAN parties with friends online or locally
- For game developers, showcase your proof-of-concept multiplayer games for testing without installation
- Create and manage your custom cloud-gaming platform using our robust API
- Establish a game server for your family to enjoy gaming on the go

## Goals

- Provide a user-friendly setup - fire and forget
- Deliver a simple and elegant interface for managing and playing your game library
- Ensure a high-quality gaming experience out-of-the-box
- Optimize for the best gaming performance right from the start

## Non-Goals

- Become a generic cloud-gaming service

## Built With

- Cloudflare Workers
- Cloudflare Pages
- Supabase
- CrosVM (with Virtio-GPU Venus and Virgl support)
- Docker
- Qwik
- Media-Over-Quic
- AWS Route53

## Known Issues

- CrosVM is still under development and needs to be merged
- Currently, the Intel dGPU, particularly the Arc A780, is the only tested and verified GPU

## Donation

If you appreciate our work and wish to support the development of Nestri, consider making a donation [here](https://polar.sh/nestri/donate). Your contributions will help us improve the platform and enhance your gaming experience. Thank you for your support!

## Demo

Nestri is still in development, but here is some footage from Behind-The-Scenes

<img src="/apps/www/public/seo/code.avif" alt="Nestri - What will you play next?">


[github-release-link]: https://github.com/nestriness/nestri/releases
[github-release-shield]: https://img.shields.io/github/v/release/nestriness/nestri?color=369eff&labelColor=black&logo=github&style=flat-square
[discord-shield]: https://img.shields.io/discord/1080111004698021909?color=5865F2&label=discord&labelColor=black&logo=discord&logoColor=white&style=flat-square
[discord-link]: https://discord.com/invite/Y6etn3qKZ3
[github-license-shield]: https://img.shields.io/github/license/nestriness/nestri?color=white&labelColor=black&style=flat-square
[github-license-link]: https://github.com/nestriness/nestri/blob/main/LICENSE
[github-stars-shield]: https://img.shields.io/github/stars/nestriness/nestri?color=ffcb47&labelColor=black&style=flat-square
[github-stars-link]: https://github.com/nestriness/nestri/network/stargazers
[share-x-shield]: https://img.shields.io/badge/-share%20on%20x-black?labelColor=black&logo=x&logoColor=white&style=flat-square
[share-x-link]: https://twitter.com/intent/tweet?text=Hey%2C%20check%20out%20this%20Github%20repository.%20It%20is%20an%20open-source%20self-hosted%20Geforce%20Now%20alternative.&url=https%3A%2F%2Fgithub.com%2Fnestriness%2Fnestri
[share-reddit-shield]: https://img.shields.io/badge/-share%20on%20reddit-black?labelColor=black&logo=reddit&logoColor=white&style=flat-square
[share-reddit-link]: https://www.reddit.com/submit?title=Hey%2C%20check%20out%20this%20Github%20repository.%20It%20is%20an%20open-source%20self-hosted%20Geforce%20Now%20alternative.&url=https%3A%2F%2Fgithub.com%2Fnestriness%2Fnestri
[image-overview]: assets/banner.png
[website-link]: https://nestri.io
[neko-url]: https://github.com/m1k1o/neko
[image-star]: assets/star-us.png
[moq-github-url]: https://quic.video
[vmaf-cuda-link]: https://developer.nvidia.com/blog/calculating-video-quality-using-nvidia-gpus-and-vmaf-cuda/