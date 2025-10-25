import './AboutPage.css'

function AboutPage() {
  return (
    <div className="about-container">
      {/* Header */}
      <header className="about-header">
        <div className="about-logo-section">
          <img src="/2.jpeg" alt="Pied Piper Icon" className="about-logo-icon" />
          <h1 className="about-team-name">Pied Piper</h1>
        </div>
      </header>

      {/* Main Content */}
      <main className="about-main">
        <h2 className="about-title">How This Works</h2>
        
        <div className="about-content">
          <section className="about-section">
            <h3 className="section-title">Our Compression Algorithm</h3>
            <p className="section-text">
              Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut 
              labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi 
              ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum 
              dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia 
              deserunt mollit anim id est laborum.
            </p>
          </section>

          <section className="about-section">
            <h3 className="section-title">Two File Formats</h3>
            <p className="section-text">
              We support two compression workflows:
            </p>
            <div className="format-grid">
              <div className="format-card">
                <h4 className="format-title">BMP to PP</h4>
                <p className="format-desc">
                  Convert standard bitmap images to our proprietary Pied Piper format 
                  for maximum compression efficiency.
                </p>
              </div>
              <div className="format-card">
                <h4 className="format-title">PP to BMP</h4>
                <p className="format-desc">
                  Decompress PP files back to standard bitmap format with perfect 
                  quality restoration.
                </p>
              </div>
            </div>
          </section>

          <section className="about-section">
            <h3 className="section-title">Simple Process</h3>
            <ol className="process-list">
              <li className="process-item">
                <span className="process-number">1</span>
                <span className="process-text">Choose your conversion type (BMP to PP or PP to BMP)</span>
              </li>
              <li className="process-item">
                <span className="process-number">2</span>
                <span className="process-text">Upload your file by dragging and dropping or browsing</span>
              </li>
              <li className="process-item">
                <span className="process-number">3</span>
                <span className="process-text">Click compress and let our algorithm work its magic</span>
              </li>
              <li className="process-item">
                <span className="process-number">4</span>
                <span className="process-text">Download your compressed or decompressed file</span>
              </li>
            </ol>
          </section>

          <section className="about-section about-section--last">
            <h3 className="section-title">Why Choose Pied Piper?</h3>
            <p className="section-text">
              Our middle-out compression technology delivers superior results compared to 
              traditional compression methods. Whether you're compressing for storage or 
              decompressing for use, Pied Piper ensures the highest quality output with 
              the smallest file sizes.
            </p>
          </section>
        </div>
      </main>
    </div>
  )
}

export default AboutPage
