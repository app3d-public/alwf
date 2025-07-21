document.addEventListener('DOMContentLoaded', () => {
  const currentPath = window.location.pathname;
  const isSecondPage = currentPath.includes('secondPage');
  const isAboutPage = currentPath.includes('about');
  
  if (!isAboutPage) {
      document.getElementById('page-indicator').textContent = isSecondPage ? 'Page 2' : 'Page 1';
      document.getElementById('prev').disabled = !isSecondPage;
      document.getElementById('next').disabled = isSecondPage;
      
      document.getElementById('prev').addEventListener('click', () => {
          if (isSecondPage) {
              window.location.href = '/';
          }
      });
      
      document.getElementById('next').addEventListener('click', () => {
          if (!isSecondPage) {
              window.location.href = '/secondPage';
          }
      });
  } else {
      document.querySelector('.btn').addEventListener('click', () => {
          window.location.href = '/';
      });
  }
});

function sendMessage() {
    Framework.send({ handler: 'btn_click', message: 'Button clicked' });
}
