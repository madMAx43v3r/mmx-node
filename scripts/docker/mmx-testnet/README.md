# Setup

To enable farming:
- Edit `compose.yml` to add your plot drives
- Create folder `~/.mmx_testnet` if not already exists
- Copy your `wallet*.dat` to `~/.mmx_testnet/wallet.dat` (the one which has your farmer key)

To enable timelord:
- Edit `compose.yml` to add `--timelord` to the command line

# Update

To get the latest image and run it:
```
docker compose pull                 # for CPU only
docker compose -f nvidia.yml pull   # for Nvidia GPU
docker compose down
```
Then start again, see below.

# Run

CPU: `docker compose up -d`

Nvidia: `docker compose -f compose.yml -f nvidia.yml up -d`

Check running: `docker compose ps`

Show logs: `docker compose logs -f`

Restart: `docker compose restart`

Note: Run without `-d` for interactive mode.
