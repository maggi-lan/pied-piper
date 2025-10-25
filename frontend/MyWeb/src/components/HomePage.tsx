import { useState } from 'react'
import './HomePage.css'

function HomePage() {
  const [menuOpen, setMenuOpen] = useState(false)

  const toggleMenu = () => {
    setMenuOpen(!menuOpen)
  }

  const handleMenuItemClick = (item: string) => {
    console.log(`${item} clicked`)
    // Add navigation logic here later
  }

  return (
    <div className="home-container">
      {/* Sidebar Menu */}
      <div className={`sidebar ${menuOpen ? 'sidebar--open' : ''}`}>
        <div className="sidebar__content">
          <h2 className="sidebar__title">Menu</h2>
          
          {/* Menu Items */}
          <nav className="sidebar__nav">
            <button 
              className="sidebar__menu-item"
              onClick={() => handleMenuItemClick('Use Our Tool')}
            >
              <span className="sidebar__menu-icon"></span>
              Use Our Tool
            </button>

            <button 
              className="sidebar__menu-item"
              onClick={() => handleMenuItemClick('About This')}
            >
              <span className="sidebar__menu-icon"></span>
              About This
            </button>

            <button 
              className="sidebar__menu-item"
              onClick={() => handleMenuItemClick('Contact Us')}
            >
              <span className="sidebar__menu-icon"></span>
              Contact Us
            </button>
          </nav>
        </div>
      </div>

      {/* Main Content Wrapper */}
      <div className={`content-wrapper ${menuOpen ? 'content-wrapper--shifted' : ''}`}>
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
              src="/1.png" 
              alt="Pied Piper Logo" 
              className="main-logo"
            />
          </div>
        </main>
      </div>

      {/* Hamburger Menu Button - Fixed Position, Always Visible */}
      <button 
        className={`menu-toggle ${menuOpen ? 'menu-toggle--active' : ''}`}
        onClick={toggleMenu}
        aria-label="Toggle menu"
      >
        <span className="menu-toggle__line"></span>
        <span className="menu-toggle__line"></span>
        <span className="menu-toggle__line"></span>
      </button>
    </div>
  )
}

export default HomePage
