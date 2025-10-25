import express from 'express'
import multer from 'multer'
import cors from 'cors'
import path from 'path'
import { exec } from 'child_process'
import fs from 'fs'
import { promisify } from 'util'

const execPromise = promisify(exec)
const app = express()
const PORT = 5000

// Enable CORS for React frontend
app.use(cors({
  origin: 'http://localhost:5173' // Your React dev server
}))

// Configure file upload
const upload = multer({
  dest: 'uploads/', // Temporary storage
  limits: { fileSize: 50 * 1024 * 1024 } // 50MB limit
})

// Ensure directories exist
if (!fs.existsSync('uploads')) fs.mkdirSync('uploads')
if (!fs.existsSync('outputs')) fs.mkdirSync('outputs')

// Health check endpoint
app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', message: 'Backend server is running' })
})

// Main compression/decompression endpoint
app.post('/api/process', upload.single('file'), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: 'No file uploaded' })
    }

    const mode = req.body.mode // 'compress' or 'decompress'
    const inputPath = req.file.path
    const outputFilename = `processed_${Date.now()}.${mode === 'compress' ? 'pp' : 'bmp'}`
    const outputPath = path.join('outputs', outputFilename)

    console.log(`Processing: ${mode} - ${req.file.originalname}`)

    // Call your C executable
    const compressExecutable = path.join(__dirname, '..', 'compress')
    const command = `${compressExecutable} ${mode} ${inputPath} ${outputPath}`

    const { stdout, stderr } = await execPromise(command)

    if (stderr) {
      console.error('Compression error:', stderr)
    }

    // Check if output file was created
    if (!fs.existsSync(outputPath)) {
      throw new Error('Processing failed - no output file generated')
    }

    // Get file stats
    const inputStats = fs.statSync(inputPath)
    const outputStats = fs.statSync(outputPath)
    const compressionRatio = ((1 - outputStats.size / inputStats.size) * 100).toFixed(2)

    // Clean up input file
    fs.unlinkSync(inputPath)

    // Send response with download link
    res.json({
      success: true,
      message: 'File processed successfully',
      downloadUrl: `/api/download/${outputFilename}`,
      stats: {
        originalSize: inputStats.size,
        processedSize: outputStats.size,
        compressionRatio: mode === 'compress' ? `${compressionRatio}%` : null
      }
    })

  } catch (error) {
    console.error('Error processing file:', error)
    
    // Clean up files on error
    if (req.file && fs.existsSync(req.file.path)) {
      fs.unlinkSync(req.file.path)
    }

    res.status(500).json({
      error: 'Failed to process file',
      details: error instanceof Error ? error.message : 'Unknown error'
    })
  }
})

// Download endpoint
app.get('/api/download/:filename', (req, res) => {
  const filename = req.params.filename
  const filepath = path.join(__dirname, '..', 'outputs', filename)

  if (!fs.existsSync(filepath)) {
    return res.status(404).json({ error: 'File not found' })
  }

  res.download(filepath, filename, (err) => {
    if (err) {
      console.error('Download error:', err)
    }
    // Clean up file after download
    setTimeout(() => {
      if (fs.existsSync(filepath)) {
        fs.unlinkSync(filepath)
      }
    }, 5000) // Delete after 5 seconds
  })
})

app.listen(PORT, () => {
  console.log(`🚀 Backend server running on http://localhost:${PORT}`)
})
