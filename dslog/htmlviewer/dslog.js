$(function() {
	var ws = new WebSocket("ws://localhost:9980/dslogger");

	var $doc = $(document);
	var $wnd = $(window);
	var ipReg = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	
	var template = $('#client-template');
	var counter = document.getElementById('counter');

	function toggleClient() {
		var bts = $('#all-clients').children('button').removeClass('bt-actived');
		var logs = $('div.log-content');
		
		for(var i = 0; i < bts.length; ++ i) {
			if (bts[i] == this) {
				$(logs[i]).css('display', 'block');
				$(bts[i]).addClass('bt-actived');

			} else {
				$(logs[i]).css('display', 'none');
			}
		}

		$doc.scrollTop($doc.height());
	}

	function clearClient() {
		var $c = $(this).parent();
		$c.find('table:first').remove();
		$c.append(template.find('table').clone());
	}

	function getClientContent(cltId) {
		var clt = $('#' + cltId);
		if (clt.length == 0) {
			clt = $(document.createElement('div')).append(template.children().clone()).appendTo($('body')).attr('id', cltId).addClass('log-content');
			var lastBt = $('#all-clients')
				.append('<button title="点击切换" id="bt-' + cltId + '">Client:' + cltId + '</button>')
				.find('button:last')
				.click(toggleClient);

			clt.find('button:first').click(clearClient);

			if ($('#all-clients').children().length == 1)
				lastBt.addClass('bt-actived');
			else
				clt.hide();
		}

		return clt;
	}

	function onWS(ws) {
		ws.onopen = function()
		{
			$('.loading-background,.loading-container').remove();
		};

		ws.onmessage = function (evt) 
		{
			var txt = evt.data;
			var idEnd = txt.indexOf('|');
			var id = txt.substring(0, idEnd);

			if (!id || id.length == 0)
				id = 'default';

			txt = txt.substr(idEnd + 1);
			var a = txt.indexOf('(');
			var b = txt.indexOf(')', a + 1);

			if (txt.charCodeAt(0) == 42) {
				// * at first
				var $c = $('#' + id);
				var cmd = txt.substring(1, a);
				if (cmd == 'clear') {
					// clear 
					if ($c.find('input:checkbox:first').prop('checked')) {
						$c.find('table:first').remove();
						$c.append(template.find('table').clone());
					}

				} else if (cmd == 'close') {
					// client disconnected
					// $('#bt-' + id).remove();
					// $c.remove();
				}

			} else {			
				var $c = getClientContent(id);
				var row = $c.find('table:first')[0].insertRow(-1);

				$(document.createElement('pre')).html(txt.substr(0, a)).appendTo(row.insertCell(-1));
				$(document.createElement('pre')).html(txt.substring(a + 1, b)).appendTo(row.insertCell(-1));
				$(document.createTextNode(txt.substr(b + 1))).appendTo(row.insertCell(-1));
			}

			$doc.scrollTop($doc.height());
		};

		ws.onerror = function(e)
		{
			console.log(e);
			showMask();
			$('.loading-container input:text').val('').attr('disabled', false);
		} ;

		ws.onclose = function(e)
		{
			console.log(e);
			showMask();
		};
	}

	function showMask() {
		if (!$('.loading-background').length) {
			var $bd = $('body');
			$(document.createElement('div')).addClass('loading-background').appendTo($bd).unbind();
			$(document.createElement('div')).addClass('loading-container').appendTo($bd).unbind()
				.html('<p>日志服务端未连接……</p>请输入观察者IP或ID：<br/><input type="text" size="15" />')
				.find('input:text')
				.keydown(function(e) {
					if (e.keyCode == 13)
						$(this).blur();

				}).focus(function() {
					this.select(0, -1);

				}).blur(function() {
					var me = this;
					var id = this.value.trim();

					if (ipReg.test(id)) {
						ws = new WebSocket('ws://' + id + ':9080/dslogger');
						$(me).val('连接中...').attr('disabled', true);
						onWS(ws);		

					} else if (id.length) {
						viewerip = null;
						$.getScript('http://www.crossdk.cn/saveip.php?script=1&id=' + id, function(response, status) {
							if (status == 'success' && viewerip && ipReg.test(viewerip)) {
								ws = new WebSocket('ws://' + viewerip + ':9080/dslogger');
								$(me).val('连接中...').attr('disabled', true);
								onWS(ws);
								return ;
							}

							alert('向服务器查询该ID时失败');
						});
					}					
				});
		}
	}

	showMask();
	onWS(ws);
});