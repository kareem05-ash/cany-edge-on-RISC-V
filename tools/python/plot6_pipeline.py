"""
plot6_pipeline.py — 4-panel pipeline image visualisation (Phase 7 STUB).

Data source: imgs/*.raw via tools/python/raw_loader.py
Plot: 4-panel figure — source image / blurred / Sobel magnitude / final edges
API: generate(out_dir, img_dir="imgs", img_stem="lena_512x512")
Panel layout: 2×2 grid, common colormap "gray", titles per panel
One figure per image stem found in imgs/ directory
"""


def generate(out_dir="docs", img_dir="imgs", img_stem="lena_512x512"):
    # TODO Phase 7: import raw_loader
    # TODO Phase 7: find all *.raw files under img_dir
    # TODO Phase 7: for each stem, load raw bytes → np.ndarray uint8
    # TODO Phase 7: fig, axes = plt.subplots(2, 2, figsize=(10, 10))
    # TODO Phase 7: panels = [source, blurred, sobel_magnitude, canny_edges]
    # TODO Phase 7: for ax, img, title in zip(axes.flat, panels, titles):
    # TODO Phase 7:     ax.imshow(img, cmap="gray"); ax.set_title(title); ax.axis("off")
    # TODO Phase 7: fig.suptitle(f"Canny Pipeline — {img_stem}", fontsize=14)
    # TODO Phase 7: fig.savefig(os.path.join(out_dir, f"pipeline_{img_stem}.png"), dpi=150)
    print("[STUB] plot6_pipeline: Phase 7 not yet implemented")