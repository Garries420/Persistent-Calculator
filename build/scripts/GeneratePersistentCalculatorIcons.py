from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
OFFICIAL_ASSETS = ROOT / "installer" / "Official" / "Assets"
MASTER_PATH = OFFICIAL_ASSETS / "calculator-icon-master.png"
CALCULATOR_ASSETS = ROOT / "src" / "Calculator" / "Assets"

CALCULATOR_ASSET_PREFIXES = (
    "CalculatorAppList",
    "CalculatorLargeTile",
    "CalculatorMedTile",
    "CalculatorSmallTile",
    "CalculatorSplashScreen",
    "CalculatorStoreLogo",
    "CalculatorWideTile",
)


def prepare_master() -> Image.Image:
    image = Image.open(MASTER_PATH).convert("RGBA")
    alpha_box = image.getchannel("A").getbbox()
    if alpha_box:
        image = image.crop(alpha_box)
    return image


def render_icon(master: Image.Image, size: tuple[int, int], margin_ratio: float = 0.08) -> Image.Image:
    width, height = size
    canvas = Image.new("RGBA", size, (0, 0, 0, 0))
    available_width = max(1, int(width * (1.0 - margin_ratio * 2)))
    available_height = max(1, int(height * (1.0 - margin_ratio * 2)))
    ratio = min(available_width / master.width, available_height / master.height)
    resized = master.resize(
        (max(1, round(master.width * ratio)), max(1, round(master.height * ratio))),
        Image.Resampling.LANCZOS,
    )
    x = (width - resized.width) // 2
    y = (height - resized.height) // 2
    canvas.alpha_composite(resized, (x, y))
    return canvas


def regenerate_packaged_assets(master: Image.Image) -> int:
    count = 0
    for path in sorted(CALCULATOR_ASSETS.glob("Calculator*.png")):
        if not path.name.startswith(CALCULATOR_ASSET_PREFIXES):
            continue
        with Image.open(path) as existing:
            size = existing.size
        rendered = render_icon(master, size)
        if size[0] * size[1] > 250_000:
            rendered = rendered.quantize(
                colors=128,
                method=Image.Quantize.FASTOCTREE,
                dither=Image.Dither.NONE,
            )
        rendered.save(path, format="PNG", optimize=True)
        count += 1
    return count


def create_windows_icon(master: Image.Image) -> None:
    icon_path = OFFICIAL_ASSETS / "PersistentCalculator.ico"
    square = render_icon(master, (256, 256), margin_ratio=0.04)
    square.save(
        icon_path,
        format="ICO",
        sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    render_icon(master, (96, 96), margin_ratio=0.04).save(
        OFFICIAL_ASSETS / "PersistentCalculator-96.png",
        format="PNG",
        optimize=True,
    )


def main() -> None:
    master = prepare_master()
    create_windows_icon(master)
    count = regenerate_packaged_assets(master)
    print(f"Generated the Windows ICO and refreshed {count} packaged calculator image assets.")


if __name__ == "__main__":
    main()
