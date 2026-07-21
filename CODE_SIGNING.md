# Code signing policy

Persistent Calculator is applying for free open-source code signing through SignPath Foundation. This policy defines which builds may be signed, who is responsible for them, and how users can review the project's privacy and security behavior.

The presence of this policy does not mean that an existing release is already signed. A release should be treated as Authenticode-signed only when the signature on the downloaded executable can be verified successfully.

## Free open-source code signing

Free code signing provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).

## Signing scope

Only release artifacts that meet all of the following conditions may be submitted for signing:

- The artifact is built from source in the public [Garries420/Persistent-Calculator](https://github.com/Garries420/Persistent-Calculator) repository.
- The build is performed by the project's GitHub Actions workflow on GitHub-hosted runners.
- The artifact is produced from the public release commit and version.
- The artifact contains only project code and permitted open-source or Windows system components.
- The signing request is manually reviewed and approved.

Locally built files, artifacts built from the private development repository, externally supplied binaries, and modified release files must not be submitted under this policy.

## Project roles

Persistent Calculator is currently maintained as a single-maintainer project. The following GitHub identity is assigned to each required role:

- **Authors and committers:** [@Garries420](https://github.com/Garries420)
- **Reviewers:** [@Garries420](https://github.com/Garries420)
- **Approvers:** [@Garries420](https://github.com/Garries420)

The maintainer uses multi-factor authentication for GitHub access. SignPath access must also use multi-factor authentication.

## Release approval

Every signing request requires manual approval. Before approval, the approver must verify that the requested version, source commit, workflow run, and produced artifact belong to the intended public release.

After signing, the release process must verify the executable's Authenticode signature before publishing it. Published SHA-256 values must be calculated from the final signed executable.

## Privacy

See the project's [Privacy policy](PRIVACY.md) for the complete disclosure.

Persistent Calculator does not upload calculation history, clipboard contents, documents, Windows account information, or telemetry. Its built-in network activity is limited to checking this repository's public GitHub release information and, only after the user approves an available update, downloading the corresponding release executable. GitHub and the user's network provider may process ordinary connection information when serving those HTTPS requests.

## Security concerns

Please follow the private reporting instructions in the project's [Security policy](SECURITY.md) if a signed artifact, release workflow, or signing request appears suspicious.
