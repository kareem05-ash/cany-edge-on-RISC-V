import os, re
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

COLOR_BARS = ["#4C72B0", "#55A868", "#DD8452"]

def parse_lmul_file(path):
    if not os.path.isfile(path):
        print("[plot5] WARNING: not found:", path)
        return None
    result   = {}
    cur_lmul = None
    with open(path, encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            m = re.match(r"---\s*LMUL=m(\d+)", s, re.IGNORECASE)
            if m:
                cur_lmul = int(m.group(1))
                continue
            if cur_lmul is None:
                continue
            # Match "Gaussian RVV (m1)   32194.12"
            m2 = re.match(r"^(.+?)\s{2,}([\d]+\.[\d]+)", s)
            if m2:
                try:
                    result[cur_lmul] = float(m2.group(2))
                    cur_lmul = None
                except ValueError:
                    pass
    return result if result else None

def generate(out_dir="docs", lmul_file="docs/lmul_gaussian.txt"):
    data = parse_lmul_file(lmul_file)
    if data is None:
        print("[plot5] Skipping: missing data.")
        return
    keys   = sorted(data.keys())
    times  = [data[k] for k in keys]
    labels = [f"LMUL={k}" for k in keys]
    best   = times.index(min(times))
    fig, ax = plt.subplots(figsize=(7, 5))
    bars = ax.bar(labels, times, color=COLOR_BARS[:len(keys)], edgecolor="black", linewidth=0.7)
    for bar, t in zip(bars, times):
        ax.text(bar.get_x() + bar.get_width()/2,
                bar.get_height() + max(times)*0.01,
                f"{t:.1f} us", ha="center", va="bottom", fontsize=10)
    ax.text(bars[best].get_x() + bars[best].get_width()/2,
            bars[best].get_height()/2,
            "* best", ha="center", va="center",
            fontsize=12, fontweight="bold", color="white")
    ax.set_xlabel("LMUL Setting")
    ax.set_ylabel("Gaussian Blur Time (us)")
    ax.set_title("Gaussian 5x5 - LMUL Sweep (VLEN=256)")
    ax.grid(axis="y", linestyle="--", alpha=0.5)
    ax.text(0.5, -0.18,
            "LMUL=2 uses i32m8 accumulator (max LMUL). LMUL=4 -> register spill.",
            ha="center", transform=ax.transAxes, fontsize=9, style="italic")
    plt.tight_layout()
    out = os.path.join(out_dir, "lmul_sweep.png")
    plt.savefig(out, dpi=150, bbox_inches="tight")
    plt.close()
    print("[plot5] Saved:", out)

if __name__ == "__main__":
    generate()
