# Private TLS seed generator

Windows: extract the experimental online ZIP to a local folder, then double-click
`Generate-TLS-Seed.cmd`. Python is not required. This creates one 64-byte private
seed at `apps/nzportable/online/tls-seed.bin` inside the extracted folder.

Upload that file using **binary transfer** in your FTP client to the matching
`apps/nzportable/online/` folder on your Wii storage. Do not upload the generator
scripts to Wii. The seed folder and file must remain writable by the client.

Keep an existing seed when updating. A valid existing seed is preserved; an
invalid existing file produces an error without overwriting it. Generate a new
seed for each separate Wii. Never share the generated file or put it on GitHub.
The release ZIP deliberately contains no seed.

Alternatively, Python users can run:

```sh
python fte-online/provision_tls_seed.py /path/to/extracted/apps/nzportable/online
```

This tool does not configure FTP or transfer files itself. **Online gameplay
still does not work**: the client lists servers and establishes an initial
connection, but cannot yet complete entry into a playable game.
