# Publish this project to GitHub (and download the built VST3)

The repository is already initialised and committed on branch `main`. You just
need to create an empty GitHub repo and push to it. **No credentials of yours are
stored here** — you authenticate on your own machine.

## 1. Create an empty repo on GitHub
Go to https://github.com/new, name it e.g. `KeyDetector`, and **do not** add a
README/.gitignore/licence (this project already has them).

## 2. Push

Using the GitHub CLI (easiest):
```bash
gh repo create KeyDetector --public --source=. --remote=origin --push
```

…or with plain git (replace YOUR-USERNAME):
```bash
git remote add origin https://github.com/YOUR-USERNAME/KeyDetector.git
git branch -M main
git push -u origin main
```

If you start from the provided bundle instead of this folder:
```bash
git clone KeyDetector.bundle KeyDetector
cd KeyDetector
git remote set-url origin https://github.com/YOUR-USERNAME/KeyDetector.git   # or: git remote add origin ...
git push -u origin main
```

## 3. Download the Windows VST3
Pushing triggers the **Build Windows VST3** GitHub Action
(`.github/workflows/windows-vst3.yml`):

1. Open your repo → **Actions** tab.
2. Click the latest **Build Windows VST3** run (or run it manually via
   *Run workflow*).
3. When it finishes (green ✓), download the **KeyDetector-VST3-Windows** artifact
   from the run's *Artifacts* section.
4. Unzip it and copy the `Key Detector.vst3` folder to
   `C:\Program Files\Common Files\VST3\`.
5. In Ableton Live: *Preferences → Plug-Ins →* enable the VST3 system folder and
   **Rescan**. Drop **Key Detector** onto an **audio track** that's playing.
