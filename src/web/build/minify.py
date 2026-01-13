Import("env")
import subprocess
from pathlib import Path
import shutil

SRC = Path("src/web")
DST = Path("src/web/build")

DST.mkdir(exist_ok=True)


def run(cmd):
    subprocess.check_call(cmd, shell=True)


for f in SRC.iterdir():
    if not f.is_file():
        continue

    out = DST / f.name

    if f.suffix == ".html":
        run(
            f'html-minifier-terser "{f}" '
            "--collapse-whitespace "
            "--remove-comments "
            "--minify-css true "
            "--minify-js true "
            f'-o "{out}"'
        )

    elif f.suffix == ".css":
        run(f'cleancss -O2 "{f}" -o "{out}"')

    elif f.suffix == ".js":
        run(f'terser "{f}" -c -m -o "{out}"')

    else:
        shutil.copy(f, out)

print("Web assets minified")
