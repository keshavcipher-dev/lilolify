# 🚀 pdfshooter — Premium Client-Side PDF Workspace

**pdfshooter** is a secure, 100% serverless, client-side PDF utility suite. It enables users to compress, merge, split, watermark, encrypt, and convert PDF documents directly inside their browser sandbox. 

By executing all processing locally, **pdfshooter** is fully immune to backend server DDoS attacks, scales infinitely at zero hosting cost, and ensures total document privacy.

---

## ✨ Features

### 1. 📉 PDF Shortener (Compressor)
* **DPI Scaling:** Resizes and rasterizes documents dynamically between 72 DPI and 300 DPI.
* **JPEG Quality Matrix:** Adjusts image compression levels (from 10% up to 100%) to shrink layout payloads while retaining vector text crispness.
* **Size Targetpres:** Standard presets (High Quality, Balanced, Max Compression) alongside granular custom slider adjustments.

### 2. 🔀 Split & Merge Workspace
* **Merger:** Queue multiple PDFs, rearrange order easily, and stitch them together.
* **Splitter:** Extract pages using precise index numbers (e.g. `1, 3`) or specific ranges (e.g. `2-5`) locally.

### 3. 🔄 Multi-Format Converter
* **PDF to Microsoft Word (.docx):** Extracts text blocks page-by-page and structures them into valid XML Document tables via `docx`.
* **PDF to Microsoft PowerPoint (.pptx):** Maps document assets and text streams onto individual slides using `pptxgenjs`.
* **PDF Rasterizer (PDF to Flat PDF):** Converts vectors and fonts to high-resolution JPEG sheets to bypass online resume system parser bugs.
* **Images to PDF:** Packs PNG and JPEG files into a single consolidated PDF document.

### 4. 🛡️ Security, Encryption & Watermarks
* **Password Encryption:** Encrypts PDFs using 128-bit security to prevent unauthorized viewing.
* **Watermark Overlay:** Layers customizable visual text watermarks with control over font size, opacity, rotation angle, and color overlays.

---

## 🔒 Security Architecture: Sandbox Security & DDoS Immunity

Most online PDF tools upload your sensitive documents (containing tax records, personal details, or contracts) to remote servers for processing. This presents two critical problems:
1. **Privacy Risk:** Your data is subject to interception or server logging.
2. **Server Overload / DDoS:** Backend engines are resource-intensive, making them prime targets for DDoS attacks.

**pdfshooter solves this entirely by running 100% offline in the client browser:**
* **Zero Network Overhead:** Files are processed locally as `ArrayBuffers` inside HTML5 canvases and JavaScript workers.
* **Infinite Scale:** Since there is no backend API, hosting is static. You can host the compiled static assets on any CDN (like **GitHub Pages**), which is natively protected by high-performance edge networks.

---

## 🛠️ Tech Stack

* **Frontend Framework:** React 18 & Vite
* **Core PDF Engine:** `pdf-lib` (Document generation, encryption, watermarking, merging)
* **PDF Parser & Visualizer:** `pdfjs-dist` (Text layers, canvas page rasterization)
* **Microsoft Office Generators:** `docx` & `pptxgenjs`
* **Icons:** `lucide-react`
* **Styling:** Custom Glassmorphic Dark Theme (Vanilla CSS)

---

## 🚀 Running Locally

Follow these commands to clone and launch the project:

```bash
# Clone the repository (Update URL once pushed to GitHub)
git clone <your-repository-url>
cd pdfshooter

# Install dependencies
npm install

# Run local development server
npm run dev
```

The application will launch on your local host (usually `http://localhost:5173`).

---

## 📦 Building & Deploying to GitHub Pages

Since **pdfshooter** compiles down to static HTML, CSS, and JS, hosting is entirely free and simple.

1. **Build the production assets:**
   ```bash
   npm run build
   ```
   This generates a minified `/dist` folder.
2. **Push the code to GitHub:**
   ```bash
   git init
   git add .
   git commit -m "feat: complete client-side pdfshooter app"
   # Add remote and push...
   ```
3. **Enable GitHub Pages:**
   * Go to your repository settings on GitHub.
   * Navigate to the **Pages** tab.
   * Under "Build and deployment", choose **GitHub Actions** or use standard **deploy from branch** (e.g. push `/dist` folder assets to `gh-pages` branch).
