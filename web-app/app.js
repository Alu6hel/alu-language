document.addEventListener('DOMContentLoaded', () => {
    const fileInput = document.getElementById('file-input');
    const dropZone = document.getElementById('drop-zone');
    const canvas = document.getElementById('preview-canvas');
    const ctx = canvas.getContext('2d');
    const statusOverlay = document.getElementById('status-overlay');
    
    const scaleSlider = document.getElementById('scale-slider');
    const scaleVal = document.getElementById('scale-val');
    const qualSlider = document.getElementById('qual-slider');
    const qualVal = document.getElementById('qual-val');
    const blurSlider = document.getElementById('blur-slider');
    const blurVal = document.getElementById('blur-val');
    
    const processBtn = document.getElementById('process-btn');
    
    let currentImage = null;

    // UI Updates
    scaleSlider.addEventListener('input', (e) => scaleVal.textContent = `${e.target.value}%`);
    qualSlider.addEventListener('input', (e) => qualVal.textContent = e.target.value);
    blurSlider.addEventListener('input', (e) => blurVal.textContent = e.target.value);

    // File Handling
    dropZone.addEventListener('dragover', (e) => {
        e.preventDefault();
        dropZone.style.borderColor = '#3b82f6';
        dropZone.style.background = 'rgba(59, 130, 246, 0.2)';
    });

    dropZone.addEventListener('dragleave', (e) => {
        e.preventDefault();
        dropZone.style.borderColor = 'var(--glass-border)';
        dropZone.style.background = 'var(--glass-bg)';
    });

    dropZone.addEventListener('drop', (e) => {
        e.preventDefault();
        dropZone.style.borderColor = 'var(--glass-border)';
        dropZone.style.background = 'var(--glass-bg)';
        if (e.dataTransfer.files.length) {
            handleFile(e.dataTransfer.files[0]);
        }
    });

    fileInput.addEventListener('change', (e) => {
        if (e.target.files.length) {
            handleFile(e.target.files[0]);
        }
    });

    function handleFile(file) {
        const url = URL.createObjectURL(file);
        currentImage = new Image();
        currentImage.onload = () => {
            canvas.width = currentImage.width;
            canvas.height = currentImage.height;
            ctx.drawImage(currentImage, 0, 0);
            canvas.classList.add('loaded');
            statusOverlay.style.display = 'none';
        };
        currentImage.src = url;
    }

    // WASM Mock Execution
    processBtn.addEventListener('click', async () => {
        if (!currentImage) {
            alert('Please select an image first.');
            return;
        }

        processBtn.classList.add('processing');
        statusOverlay.style.display = 'block';
        statusOverlay.textContent = 'Allocating WASM Memory...';
        canvas.style.opacity = '0.5';

        // Simulate WASM load & execution latency
        await new Promise(r => setTimeout(r, 500));
        statusOverlay.textContent = 'Executing Alu _apply_filter...';
        
        await new Promise(r => setTimeout(r, 800));

        // Note: In a real WASM integration, we would call:
        // const outPtr = Module.ccall('apply_filter', 'number', ['number', 'number', 'number'], [inPtr, width, height]);
        // const outArray = new Uint8ClampedArray(Module.HEAPU8.buffer, outPtr, width * height * 4);
        // ctx.putImageData(new ImageData(outArray, width, height), 0, 0);
        
        // Mocking the result by applying a CSS filter instead for demo purposes
        const scale = scaleSlider.value / 100;
        const blur = blurSlider.value;
        
        canvas.width = currentImage.width * scale;
        canvas.height = currentImage.height * scale;
        
        ctx.filter = `blur(${blur}px)`;
        ctx.drawImage(currentImage, 0, 0, canvas.width, canvas.height);
        
        canvas.style.opacity = '1';
        processBtn.classList.remove('processing');
        statusOverlay.style.display = 'none';
    });
});
