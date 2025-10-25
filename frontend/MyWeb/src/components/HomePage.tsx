import './HomePage.css'

function HomePage() {
  return (
    <div className="home-container">
      {/* Header with logo and team name */}
      <header className="header">
        <div className="logo-section">
          <img src="/2.jpeg" alt="Pied Piper Icon" className="logo-icon" />
          <h1 className="team-name">Pied Piper</h1>
        </div>
      </header>

      {/* Main content */}
      <main className="main-content">
        <div className="content-left">
          <h2 className="main-title">Compress your Image</h2>
          
          <div className="button-group">
            <button className="compress-btn">
              Compress Here
            </button>

            <button className="how-it-works-btn">
              How this works
            </button>
          </div>
        </div>

        <div className="content-right">
          <img 
            src="/1.jpg" 
            alt="Pied Piper Logo" 
            className="main-logo"
          />
        </div>
      </main>
    </div>
  )
}

export default HomePage
