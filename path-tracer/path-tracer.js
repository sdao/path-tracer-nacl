var startTime = new Date();

function handleMessage(message) {
  document.getElementById('iterationCount').textContent = message.data;

  var curTime = new Date();
  var timeString = getTimeString(curTime - startTime);
  document.getElementById('timer').textContent = timeString;
}

function getTimeString(duration) {
  const SECOND = 1000;
  const MINUTE = 60 * SECOND;
  const HOUR = 60 * MINUTE;
  const DAY = 24 * HOUR;

  var days = Math.floor(duration / DAY);
  duration -= (days * DAY);

  var hours = Math.floor(duration / HOUR);
  duration -= (hours * HOUR);

  var minutes = Math.floor(duration / MINUTE);
  duration -= (minutes * MINUTE);

  var seconds = Math.floor(duration / SECOND);

  var formatString = "";

  if (days > 0) {
    formatString += (days + "d ");
  }

  if (hours > 0 || formatString.length > 0) {
    formatString += (hours + "h ");
  }

  if (minutes > 0 || formatString.length > 0) {
    formatString += (minutes + "min ");
  }

  formatString += (seconds + "s ");

  return formatString.slice(0, formatString.length - 1);
}
