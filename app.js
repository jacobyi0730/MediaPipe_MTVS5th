import {
  DrawingUtils,
  FilesetResolver,
  HandLandmarker,
} from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.15/+esm";

const MODEL_URL =
  "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task";
const WASM_URL =
  "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.15/wasm";

const video = document.getElementById("webcam");
const canvas = document.getElementById("overlay");
const context = canvas.getContext("2d");
const startButton = document.getElementById("start-button");
const gestureLabel = document.getElementById("gesture-label");
const debugLabel = document.getElementById("debug-label");
const cameraStatus = document.getElementById("camera-status");
const modelStatus = document.getElementById("model-status");

let handLandmarker = null;
let drawingUtils = null;
let lastVideoTime = -1;

const gestureNames = {
  rock: "Fist",
  scissors: "Scissors",
  paper: "Paper",
  unknown: "Unknown",
};

function distance(a, b) {
  return Math.hypot(a.x - b.x, a.y - b.y);
}

function angleBetween(a, b, c) {
  const abx = a.x - b.x;
  const aby = a.y - b.y;
  const cbx = c.x - b.x;
  const cby = c.y - b.y;

  const dot = abx * cbx + aby * cby;
  const mag1 = Math.hypot(abx, aby);
  const mag2 = Math.hypot(cbx, cby);

  if (!mag1 || !mag2) {
    return 0;
  }

  const cosine = Math.min(1, Math.max(-1, dot / (mag1 * mag2)));
  return Math.acos(cosine) * (180 / Math.PI);
}

function getPalmSize(landmarks) {
  return Math.max(
    distance(landmarks[0], landmarks[5]),
    distance(landmarks[0], landmarks[17]),
    0.001,
  );
}

function getFingerState(landmarks, mcpIndex, pipIndex, dipIndex, tipIndex) {
  const pipAngle = angleBetween(
    landmarks[mcpIndex],
    landmarks[pipIndex],
    landmarks[dipIndex],
  );
  const dipAngle = angleBetween(
    landmarks[pipIndex],
    landmarks[dipIndex],
    landmarks[tipIndex],
  );
  const tipReach = distance(landmarks[tipIndex], landmarks[0]);
  const pipReach = distance(landmarks[pipIndex], landmarks[0]);

  return {
    extended:
      pipAngle > 160 &&
      dipAngle > 150 &&
      tipReach > pipReach * 1.12,
    pipAngle,
    dipAngle,
    tipReach,
    pipReach,
  };
}

function getThumbState(landmarks, palmSize) {
  const mcpAngle = angleBetween(landmarks[1], landmarks[2], landmarks[3]);
  const ipAngle = angleBetween(landmarks[2], landmarks[3], landmarks[4]);
  const spread = distance(landmarks[4], landmarks[5]) / palmSize;
  const reach = distance(landmarks[4], landmarks[0]) / palmSize;

  return {
    extended: ipAngle > 145 && mcpAngle > 135 && spread > 0.55 && reach > 1.1,
    mcpAngle,
    ipAngle,
    spread,
    reach,
  };
}

function classifyGesture(landmarks) {
  const palmSize = getPalmSize(landmarks);
  const thumb = getThumbState(landmarks, palmSize);
  const index = getFingerState(landmarks, 5, 6, 7, 8);
  const middle = getFingerState(landmarks, 9, 10, 11, 12);
  const ring = getFingerState(landmarks, 13, 14, 15, 16);
  const pinky = getFingerState(landmarks, 17, 18, 19, 20);

  const longFingersExtended = [index, middle, ring, pinky].filter(
    (finger) => finger.extended,
  ).length;

  let gesture = "unknown";

  if (longFingersExtended === 0 && thumb.reach < 1.25) {
    gesture = "rock";
  } else if (
    index.extended &&
    middle.extended &&
    !ring.extended &&
    !pinky.extended
  ) {
    gesture = "scissors";
  } else if (
    longFingersExtended === 4 &&
    (thumb.extended || thumb.spread > 0.45)
  ) {
    gesture = "paper";
  }

  return {
    gesture,
    details: {
      thumb,
      index,
      middle,
      ring,
      pinky,
      longFingersExtended,
    },
  };
}

function resizeCanvas() {
  canvas.width = video.videoWidth;
  canvas.height = video.videoHeight;
}

function roundRect(ctx, x, y, width, height, radius) {
  ctx.beginPath();
  ctx.moveTo(x + radius, y);
  ctx.lineTo(x + width - radius, y);
  ctx.quadraticCurveTo(x + width, y, x + width, y + radius);
  ctx.lineTo(x + width, y + height - radius);
  ctx.quadraticCurveTo(x + width, y + height, x + width - radius, y + height);
  ctx.lineTo(x + radius, y + height);
  ctx.quadraticCurveTo(x, y + height, x, y + height - radius);
  ctx.lineTo(x, y + radius);
  ctx.quadraticCurveTo(x, y, x + radius, y);
  ctx.closePath();
}

function drawGestureBadge(text) {
  const label = `Gesture: ${text}`;

  context.save();
  context.fillStyle = "rgba(10, 15, 20, 0.72)";
  context.strokeStyle = "rgba(125, 249, 198, 0.4)";
  context.lineWidth = 2;
  roundRect(context, 18, 18, 230, 52, 18);
  context.fill();
  context.stroke();
  context.fillStyle = "#edf6ff";
  context.font = "700 24px 'Space Grotesk'";
  context.fillText(label, 34, 52);
  context.restore();
}

async function createLandmarker() {
  modelStatus.textContent = "Loading";

  const vision = await FilesetResolver.forVisionTasks(WASM_URL);
  handLandmarker = await HandLandmarker.createFromOptions(vision, {
    baseOptions: {
      modelAssetPath: MODEL_URL,
    },
    runningMode: "VIDEO",
    numHands: 1,
    minHandDetectionConfidence: 0.6,
    minHandPresenceConfidence: 0.6,
    minTrackingConfidence: 0.6,
  });

  drawingUtils = new DrawingUtils(context);
  modelStatus.textContent = "Ready";
}

async function startWebcam() {
  if (!handLandmarker) {
    await createLandmarker();
  }

  const stream = await navigator.mediaDevices.getUserMedia({
    video: {
      facingMode: "user",
      width: { ideal: 1280 },
      height: { ideal: 720 },
    },
    audio: false,
  });

  video.srcObject = stream;
  await video.play();
  resizeCanvas();

  cameraStatus.textContent = "Running";
  startButton.disabled = true;
  debugLabel.textContent = "Show one hand clearly inside the frame.";
  window.requestAnimationFrame(predictLoop);
}

function predictLoop() {
  if (!handLandmarker || video.readyState < HTMLMediaElement.HAVE_CURRENT_DATA) {
    window.requestAnimationFrame(predictLoop);
    return;
  }

  if (video.currentTime !== lastVideoTime) {
    lastVideoTime = video.currentTime;

    const results = handLandmarker.detectForVideo(video, performance.now());
    context.clearRect(0, 0, canvas.width, canvas.height);

    if (results.landmarks.length > 0) {
      const landmarks = results.landmarks[0];
      const handedness = results.handedness[0]?.[0]?.categoryName ?? "Right";
      const { gesture, details } = classifyGesture(landmarks);
      const gestureText = gestureNames[gesture];

      drawingUtils.drawConnectors(landmarks, HandLandmarker.HAND_CONNECTIONS, {
        color: "#7df9c6",
        lineWidth: 4,
      });
      drawingUtils.drawLandmarks(landmarks, {
        color: "#ffb86c",
        lineWidth: 2,
        radius: 5,
      });
      drawGestureBadge(gestureText);

      gestureLabel.textContent = gestureText;
      debugLabel.textContent =
        `Detected: ${gestureText} (${handedness}) | ` +
        `extended=${details.longFingersExtended} ` +
        `thumbSpread=${details.thumb.spread.toFixed(2)}`;
    } else {
      gestureLabel.textContent = "No hand";
      debugLabel.textContent = "Move your hand into the camera view.";
    }
  }

  window.requestAnimationFrame(predictLoop);
}

startButton.addEventListener("click", async () => {
  cameraStatus.textContent = "Requesting access";

  try {
    await startWebcam();
  } catch (error) {
    console.error(error);
    cameraStatus.textContent = "Error";
    modelStatus.textContent = "Failed";
    gestureLabel.textContent = "Startup failed";
    debugLabel.textContent = "Check camera permission and network access.";
    startButton.disabled = false;
  }
});
