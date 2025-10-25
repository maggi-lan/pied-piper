import { useState } from 'react'
import './UploadPage.css'

type FileType = 'bmp' | 'pp'

function UploadPage() {
  const [selectedFile, setSelectedFile] = useState<File | null>(null)
  const [isDragging, setIsDragging] = useState(false)
  const [fileType, setFileType] = useState<FileType>('bmp')

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    if (event.target.files && event.target.files[0]) {
      setSelectedFile(event.target.files[0])
    }
  }

  const handleDragOver = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault()
    setIsDragging(true)
  }

  const handleDragLeave = () => {
    setIsDragging(false)
  }

  const handleDrop = (event: React.DragEvent<HTMLDivElement>) => {
    event.preventDefault()
    setIsDragging(false)
    
    if (event.dataTransfer.files && event.dataTransfer.files[0]) {
      setSelectedFile(event.dataTransfer.files[0])
    }
  }

  const handleCompress = () => {
    if (selectedFile) {
      console.log('Compressing:', selectedFile.name, 'Type:', fileType)
      // Add your compression logic here
    }
  }

  const handleRemove = () => {
    setSelectedFile(null)
  }

  const getAcceptedFiles = () => {
    return fileType === 'bmp' ? '.bmp' : '.pp'
  }

  const getFileTypeLabel = () => {
    return fileType === 'bmp' ? 'BMP' : 'PP'
  }

  return (
    <div className="upload-container">
      {/* Header */}
      <header className="upload-header">
        <div className="upload-logo-section">
          <img src="/2.jpeg" alt="Pied Piper Icon" className="upload-logo-icon" />
          <h1 className="upload-team-name">Pied Piper</h1>
        </div>
      </header>

      {/* Main Content */}
      <main className="upload-main">
        <h2 className="upload-title">Upload Your Image</h2>
        <p className="upload-subtitle">Compress or decompress your images with our advanced algorithm</p>

        {/* File Type Toggle */}
        <div className="file-type-toggle">
          <button
            className={`toggle-btn ${fileType === 'bmp' ? 'toggle-btn--active' : ''}`}
            onClick={() => {
              setFileType('bmp')
              setSelectedFile(null) // Clear selected file when switching
            }}
          >
            .bmp to .pp
          </button>
          <button
            className={`toggle-btn ${fileType === 'pp' ? 'toggle-btn--active' : ''}`}
            onClick={() => {
              setFileType('pp')
              setSelectedFile(null) // Clear selected file when switching
            }}
          >
            .pp to .bmp
          </button>
        </div>

        {/* Upload Area */}
        <div
          className={`upload-area ${isDragging ? 'upload-area--dragging' : ''} ${
            selectedFile ? 'upload-area--has-file' : ''
          }`}
          onDragOver={handleDragOver}
          onDragLeave={handleDragLeave}
          onDrop={handleDrop}
        >
          {!selectedFile ? (
            <>
              <div className="upload-icon">📁</div>
              <p className="upload-text">Drag and drop your {getFileTypeLabel()} file here</p>
              <p className="upload-or">or</p>
              <label htmlFor="file-input" className="upload-browse-btn">
                Browse Files
              </label>
              <input
                id="file-input"
                type="file"
                accept={getAcceptedFiles()}
                onChange={handleFileSelect}
                style={{ display: 'none' }}
              />
              <p className="upload-hint">Supports: {getFileTypeLabel()} files only</p>
            </>
          ) : (
            <div className="file-preview">
              <div className="file-icon">✓</div>
              <div className="file-info">
                <p className="file-name">{selectedFile.name}</p>
                <p className="file-size">
                  {(selectedFile.size / 1024 / 1024).toFixed(2)} MB
                </p>
                <p className="file-type-badge">{getFileTypeLabel()} File</p>
              </div>
              <button className="remove-btn" onClick={handleRemove}>
                ✕
              </button>
            </div>
          )}
        </div>

        {/* Compress Button */}
        {selectedFile && (
          <button className="compress-action-btn" onClick={handleCompress}>
            Compress {getFileTypeLabel()} Image
          </button>
        )}
      </main>
    </div>
  )
}

export default UploadPage
