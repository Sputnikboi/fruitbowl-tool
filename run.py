#!/usr/bin/env python3
"""Launch the Fruitbowl Resource Pack Tool."""
import sys


def main_cli():
    """CLI entry point for deploy and scan commands."""
    import argparse
    from fruitbowl_tool.deploy import (
        zip_pack, compute_sha1, update_server_properties,
        upload_to_github, get_pack_url, deploy_full,
    )

    parser = argparse.ArgumentParser(
        description="Fruitbowl Resource Pack Tool — CLI")
    sub = parser.add_subparsers(dest="command")

    # deploy sub-command
    deploy_p = sub.add_parser("deploy", help="Zip, upload to GitHub, update server.properties")
    deploy_p.add_argument("pack", help="Path to resource pack directory")
    deploy_p.add_argument("--server", "-s", required=True,
                          help="Path to Minecraft server directory")
    deploy_p.add_argument("--restart", "-r", action="store_true",
                          help="Restart minecraft.service after deploy")

    # zip sub-command
    zip_p = sub.add_parser("zip", help="Zip pack and show SHA1")
    zip_p.add_argument("pack", help="Path to resource pack directory")
    zip_p.add_argument("--output", "-o", help="Output zip path")

    # scan sub-command
    scan_p = sub.add_parser("scan", help="Scan pack models for z-fighting")
    scan_p.add_argument("pack", help="Path to resource pack directory")
    scan_p.add_argument("--min-overlap", "-m", type=float, default=0.5,
                        help="Minimum overlap area to report (default: 0.5)")

    args = parser.parse_args()

    if args.command == "zip":
        zip_path = zip_pack(args.pack, args.output)
        sha1 = compute_sha1(zip_path)
        import os
        size = os.path.getsize(zip_path)
        print(f"✓ Zip: {zip_path} ({size // 1024} KB)")
        print(f"✓ SHA1: {sha1}")
        print(f"✓ URL: {get_pack_url()}")

    elif args.command == "deploy":
        msgs = deploy_full(args.pack, args.server)
        for tag, msg in msgs:
            print(f"  {msg}")

        if args.restart:
            import subprocess
            print()
            print("Restarting minecraft.service…")
            result = subprocess.run(
                ["systemctl", "--user", "restart", "minecraft.service"],
                capture_output=True, text=True)
            if result.returncode == 0:
                print("✓ Server restarted.")
            else:
                print(f"⚠ Restart failed: {result.stderr.strip()}")

    elif args.command == "scan":
        from fruitbowl_tool.zfight import scan_pack, format_report
        results = scan_pack(args.pack)
        filtered = {}
        for model, hits in results.items():
            big_hits = [h for h in hits if h.overlap_area >= args.min_overlap]
            if big_hits:
                filtered[model] = big_hits
        print(format_report(filtered))

    else:
        parser.print_help()


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] in ("deploy", "zip", "scan"):
        main_cli()
    else:
        from fruitbowl_tool.gui import main
        main()
