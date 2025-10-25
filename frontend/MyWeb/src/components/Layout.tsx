import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import './Layout.css'

interface LayoutProps {
  children: React.ReactNode
}

function Layout({ children }: LayoutProps) {
  const navigate = useNavigate()
  const [menuOpen, setMenuOpen] = useState(false)

  const toggleMenu = () => {
    setMenuOpen(!menuOpen)
  }

  const handleNavigation = (path: string) => {
    navigate(path)
    setMenuOpen(false)
  }

  return (
    <div className="layout-container">
      {/* Sidebar Menu */}
      <div className={`sidebar ${menuOpen ? 'sidebar--open' : ''}`}>
        <div className="sidebar__content">
          <h2 className="sidebar__title">Menu</h2>
          
          <nav className="sidebar__nav">
            <button 
              className="sidebar__menu-item"
              onClick={() => handleNavigation('/')}
            >
              <span className="sidebar__menu-icon"></span>
              Home
            </button>

            <button 
              className="sidebar__menu-item"
              onClick={() => handleNavigation('/upload')}
            >
              <span className="sidebar__menu-icon"></span>
              Use Our Tool
            </button>

            <button 
              className="sidebar__menu-item"
              onClick={() => handleNavigation('/about')}
            >
              <span className="sidebar__menu-icon"></span>
              About This
            </button>

            <button 
              className="sidebar__menu-item"
              onClick={() => handleNavigation('/contact')}
            >
              <span className="sidebar__menu-icon"></span>
              Contact Us
            </button>
          </nav>
        </div>
      </div>

      {/* Hamburger Menu Button */}
      <button 
        className={`menu-toggle ${menuOpen ? 'menu-toggle--active' : ''}`}
        onClick={toggleMenu}
        aria-label="Toggle menu"
      >
        <span className="menu-toggle__line"></span>
        <span className="menu-toggle__line"></span>
        <span className="menu-toggle__line"></span>
      </button>

      {/* Page Content */}
      {children}
    </div>
  )
}

export default Layout
