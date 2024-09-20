# Contributor Covenant Code of Conduct

## Our Pledge

In the interest of fostering an open and welcoming environment, we as contributors and maintainers pledge to making participation in our project and our community a harassment-free experience for everyone, regardless of age, body size, disability, ethnicity, sex characteristics, gender identity and expression, level of experience, education, socio-economic status, nationality, personal appearance, race, religion, or sexual identity and orientation.

## Our Standards

Examples of behavior that contributes to creating a positive environment include:

* Using welcoming and inclusive language
* Being respectful of differing viewpoints and experiences
* Gracefully accepting constructive criticism
* Focusing on what is best for the community
* Showing empathy towards other community members

Examples of unacceptable behavior by participants include:

* The use of sexualized language or imagery and unwelcome sexual attention or advances
* Trolling, insulting/derogatory comments, and personal or political attacks
* Public or private harassment
* Publishing others' private information, such as a physical or electronic address, without explicit permission
* Other conduct which could reasonably be considered inappropriate in a professional setting

## Our Responsibilities

Project maintainers are responsible for clarifying the standards of acceptable behavior and are expected to take appropriate and fair corrective action in response to any instances of unacceptable behavior.

Project maintainers have the right and responsibility to remove, edit, or reject comments, commits, code, wiki edits, issues, and other contributions that are not aligned to this Code of Conduct, or to ban temporarily or permanently any contributor for other behaviors that they deem inappropriate

## Project Structure

Nestri is organized as a monorepo using Turborepo. Here's an overview of the main directories and their purposes:

### Root Directory

- `apps/`: Contains the main applications
  - `www/`: The main Nestri website built with Qwik
- `infra/`: Contains the relevant files to deploy the app using [SST](https://sst.dev)
- `packages/`: Shared packages and configurations
  - `api/`: Core API for Nestri
  - `eslint-config/`: Shared ESLint configurations
  - `typescript-config/`: Shared TypeScript configurations
  - `ui/`: Shared UI components and styles

### Key Files

- `package.json`: Root package file defining workspaces and shared dev dependencies
- `turbo.json`: Turborepo configuration
- `LICENSE`: GNU Affero General Public License v3.0

### Apps

#### www (Nestri Website)

This is the Nestri website hosted on Cloudflare Pages

### Packages

#### api (Nestri Core)

The core API for Nestri, built with Hono and deployed to Cloudflare Workers.

#### eslint-config

Shared ESLint configurations for maintaining consistent code style across the project.

#### typescript-config

Shared TypeScript configurations to ensure consistent TypeScript settings across the project.

#### ui

Shared UI components and styles used across the Nestri project.

### Infrastructure

- `infra/`: Contains infrastructure-as-code files
  - `www.ts`: Defines the deployment configuration for the Nestri website

### Development

When working on Nestri, you'll primarily be dealing with the `apps/www` directory for the main website and the various packages in the `packages/` directory for shared functionality.

For more detailed information about each package or app, refer to their respective README files or package.json scripts.
