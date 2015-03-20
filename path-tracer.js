var startTime = new Date();

function handleMessage(message) {
  document.getElementById('iterationCount').textContent = message.data;

  var curTime = new Date();
  var diff = Math.round((curTime - startTime) / 1000);
  document.getElementById('timer').textContent = diff + " seconds";
}
