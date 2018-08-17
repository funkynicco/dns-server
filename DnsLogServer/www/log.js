function TableRow() {
	this.html = '';
	
	this.addColumn = function (value) {
		this.html += '<td>' + value + '</td>';
	}
	
	this.toString = function () {
		return '<tr>' + this.html + '</tr>';
	}
}

function Table() {
	this.rows = [];
	
	this.addRow = function (row) {
		this.rows.push(row);
	}
	
	this.getRowsHtml = function () {
		var html = '';
		
		for (var i = 0; i < this.rows.length; ++i) {
			html += '<tr>';
			html += this.rows[i].toString();
			html += '</tr>';
		}
		
		return html;
	}
}

function padLeft(str, num, ch) {
	while (str.length < num) {
		str = ch + str;
	}
	return str;
}

function getSimplifiedTime(date) {
	return padLeft(date.getHours().toString(), 2, '0') + ':' +
		padLeft(date.getMinutes().toString(), 2, '0') + ':' +
		padLeft(date.getSeconds().toString(), 2, '0');
}

$(function() {
	$('.table-container table tbody').html('Loading ...');
	
	$.ajax({
		url: '/api/log',
		success: function(result) {
			var table = new Table();
			
			for (var i = 0; i < result.logs.length; ++i) {
				var row = new TableRow();
				
				var date = new Date(result.logs[i][2] * 1000);
				
				row.addColumn(getSimplifiedTime(date)); // tmTime
				row.addColumn(result.logs[i][1]); // dwThread
				row.addColumn(result.logs[i][3] + ':' + result.logs[i][4]); // filepath:line
				row.addColumn(result.logs[i][5]); // message
				
				table.addRow(row);
			}
			
			/*var html = '';
			for (var i = 0; i < result.logs.length; ++i) {
				html += '<tr><td>'+result.logs[i][2]+'</td><td>1</td><td>1</td><td>1</td></tr>';
			}*/
			
			$('.table-container table tbody').html(table.getRowsHtml());
			processTableColoring(); // code.js
		},
		error: function(xhr, status, error) {
			console.log(xhr);
			console.log(status);
			console.log(error);
		}
	})
})