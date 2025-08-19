static const String HTML_TEXT = R"(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <script type="text/javascript">
      window.onload = function() {
        // imgのsrcに「http://192.168.x.x:81/stream」のurlを設定
        var id_stream = document.getElementById("img_stream");
        id_stream.src = document.location.origin + ":81/stream";
      };

    // キャプチャ停止
    function stop() {
      fetch('/button_stop');
      document.getElementById("auto").style.display  ="inline";
      document.getElementById("stop").style.display  ="none";
      document.getElementById("left").style.display  ="inline";
      document.getElementById("right").style.display ="inline";
      document.getElementById("up").style.display    ="inline";
      document.getElementById("down").style.display  ="inline";
    }

    // 定期的にキャプチャ取得
    function auto() {
      fetch('/button_auto');
      document.getElementById("auto").style.display  ="none";
      document.getElementById("stop").style.display  ="inline";
      document.getElementById("left").style.display  ="none";
      document.getElementById("right").style.display ="none";
      document.getElementById("up").style.display    ="none";
      document.getElementById("down").style.display  ="none";
    }
    </script>
    <style>
      html {
        touch-action: manipulation;
      }
      div {
        text-align: center;
      }
      button {
        font-size: 32px;
        margin: 10px;
        width: 100px;
        height: 60px;
        border: none;
        outline: none;
        background-color: #FFFFFF55;
      }
      
      .container {
        display: flex;
        align-items: center;
        flex-direction: column;
      }
      .container img {
      }
      .container div {
        align-items: center;
        position: relative;
        bottom: 100px;
      }
      .container canvas {
        position: relative;
      }
    </style>
  </head>
  
  <body>
    <div class="container">
      <img id="img_stream" width="800" height="600">
      <div>
        <button id="left"  onclick="fetch('/button_left');">←</button>
        <button id="right" onclick="fetch('/button_right');">→</button>
        <button id="auto"  onclick="auto();" style="display:inline;">📡</button>
        <button id="stop"  onclick="stop();" style="display:none;">❌</button>
        <button id="up"    onclick="fetch('/button_up');">↑</button>
        <button id="down"  onclick="fetch('/button_down');">↓</button>
      </div>
    </div>
  </body>
  
</html>
)";
