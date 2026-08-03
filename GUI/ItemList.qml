import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import BackEnd 1.0

Rectangle {
	id: itemList
	property alias innerModel: theView.model
	property string imageSourceTemplate
	property int curIndex: -1
	property bool noImage: false
	property bool hideNoImageSymbol: false
	property bool editable: false
	property bool alwaysShowEditActions: false
	property color selectedBackgroundColor: "transparent"
	property color selectedTextColor: "white"
	property color unselectedTextColor: "white"
	property color itemBackgroundColor: "transparent"
	property color selectedBorderColor: "transparent"
	property color itemBorderColor: "transparent"
	property int selectedBorderWidth: 0
	property int itemBorderWidth: 0
	property bool keepSelectedItemAtTop: false
	property int selectedItemRowsAbove: 2
	property int noAutoScrollItemId: -1
	property bool scrollSelectsItem: true
	property int itemFontPixelSize: 18
	property int itemCornerRadius: 8
	property int listItemHeight: 100
	property int listItemSpacing: 10
	property bool showExpandIndicator: false
	property int expandIndicatorActiveIndex: -1
	property bool suppressPositionOnIndexChange: false

	signal newIndexSelected(int newIndex)
	signal indexHighlighted(int newIndex)
	signal deleteItem(int index)
	signal editItemName(int index, string name)


	color: "transparent"

	Timer {
		id: repositionTimer
		interval: 0
		onTriggered: itemList.positionSelectedItem()
	}

	onCurIndexChanged: {
		if (theView.currentIndex !== curIndex && curIndex >= -1) {
			suppressPositionOnIndexChange = true
			theView.currentIndex = curIndex
			suppressPositionOnIndexChange = false
		}
		if (keepSelectedItemAtTop)
			repositionTimer.restart()
	}

	function selectIndex(index) {
		if (!theView.model || index < 0 || index >= theView.count)
			return
		if (isIndexLocked(index))
			return
		if (curIndex !== index)
			curIndex = index
		else if (keepSelectedItemAtTop)
			positionSelectedItem()
		newIndexSelected(index)
		// Re-apply after ListView finishes any internal currentIndex handling.
		if (keepSelectedItemAtTop)
			Qt.callLater(itemList.positionSelectedItem)
	}

	function isIndexLocked(index) {
		if (!theView.model || index < 0 || index >= theView.count
				|| typeof theView.model.get !== "function") {
			return false
		}
		return theView.model.get(index).locked === true
	}

	function nextSelectableIndex(fromIndex, step) {
		var index = fromIndex + step
		while (index >= 0 && index < theView.count) {
			if (!isIndexLocked(index))
				return index
			index += step
		}
		return -1
	}

	function highlightIndex(index) {
		if (!theView.model || index < 0 || index >= theView.count)
			return
		if (curIndex !== index)
			curIndex = index
		else if (keepSelectedItemAtTop)
			positionSelectedItem()
		else if (theView.count > 0)
			theView.positionViewAtIndex(index, ListView.Contain)
		indexHighlighted(index)
	}

	function scrollUp() {
		if (theView.count <= 0 || curIndex <= 0)
			return
		var nextIndex = scrollSelectsItem ? nextSelectableIndex(curIndex, -1)
		                                  : curIndex - 1
		if (nextIndex < 0)
			return
		if (scrollSelectsItem)
			selectIndex(nextIndex)
		else
			highlightIndex(nextIndex)
	}

	function scrollDown() {
		if (theView.count <= 0)
			return
		var nextIndex = scrollSelectsItem
				? nextSelectableIndex(curIndex < 0 ? -1 : curIndex, 1)
				: (curIndex < 0 ? 0 : curIndex + 1)
		if (nextIndex >= theView.count)
			return
		if (nextIndex < 0)
			return
		if (scrollSelectsItem)
			selectIndex(nextIndex)
		else
			highlightIndex(nextIndex)
	}

	function currentItemId() {
		if (!theView.model || curIndex < 0 || curIndex >= theView.count
				|| typeof theView.model.get !== "function") {
			return -1
		}
		return parseInt(theView.model.get(curIndex).itemId)
	}

	function positionSelectedItem() {
		if (!keepSelectedItemAtTop || theView.count <= 0) {
			return
		}

		if (currentItemId() === noAutoScrollItemId) {
			theView.contentY = theView.originY
			return
		}

		var index = curIndex
		if (index < 0 || index >= theView.count) {
			return
		}

		// Keep the first items pinned to the top: never leave a blank gap above.
		if (index <= selectedItemRowsAbove) {
			theView.contentY = theView.originY
			return
		}

		var rowStride = listItemHeight + listItemSpacing
		var targetY = (index - selectedItemRowsAbove) * rowStride
		var maxContentY = Math.max(0, theView.contentHeight - theView.height)
		theView.contentY = theView.originY + Math.min(targetY, maxContentY)
	}
	ColumnLayout {
		id: layout
		anchors.fill: parent
		anchors.margins: 5
		spacing: 5

		ListView {
			id: theView
			Layout.fillHeight: true
			Layout.fillWidth: true
			layoutDirection: Qt.LeftToRight
			verticalLayoutDirection: ListView.TopToBottom
			displayMarginBeginning: 15
			displayMarginEnd: 15
			spacing: listItemSpacing
			clip: true
			// When we manage contentY ourselves, ListView must not auto-scroll the
			// current item (it can push the first rows to the bottom and leave a gap on top).
			highlightFollowsCurrentItem: !itemList.keepSelectedItemAtTop
			boundsBehavior: Flickable.StopAtBounds
			onCurrentIndexChanged: {
				// Selection highlight is driven by curIndex, not ListView.currentIndex.
				// Model rebuilds reset currentIndex and must not move the viewport alone.
				if (!itemList.suppressPositionOnIndexChange
						&& theView.currentIndex === itemList.curIndex)
					itemList.positionSelectedItem()
			}
			onHeightChanged: {
				if (itemList.keepSelectedItemAtTop)
					repositionTimer.restart()
			}
			onCountChanged: {
				if (itemList.keepSelectedItemAtTop)
					repositionTimer.restart()
			}
			onContentHeightChanged: {
				if (itemList.keepSelectedItemAtTop)
					repositionTimer.restart()
			}

			footer: Item {
				width: theView.width
				height: itemList.keepSelectedItemAtTop
						? Math.max(0, theView.height
							- (itemList.selectedItemRowsAbove + 1) * itemList.listItemHeight
							- itemList.selectedItemRowsAbove * theView.spacing)
						: 0
			}

			delegate: Rectangle {
				id: itemRoot
				property bool isSelected: (index === itemList.curIndex)
				readonly property bool locked: model.locked === true
				readonly property bool showImage: !noImage && imageSourceTemplate !== "" && itemImage.status === Image.Ready
				readonly property bool reserveLeftSymbolSpace: noImage && !hideNoImageSymbol
				readonly property bool showEditPanel: itemList.editable && !locked
				height: itemList.listItemHeight
				width: ListView.view.width
				radius: itemList.itemCornerRadius
				color: isSelected ? selectedBackgroundColor : itemBackgroundColor
				border.width: isSelected ? selectedBorderWidth : itemBorderWidth
				border.color: isSelected ? selectedBorderColor : itemBorderColor
				opacity: locked ? 0.58 : 1.0
				Rectangle {
					id: itemImageRectBorder
					width: 10
					color: "transparent"

					anchors {
						top:parent.top
						bottom: parent.bottom
						right: itemImageRect.right
					}
				}
				Rectangle {
					id: itemNameRectBorder
					width: 10
					color: "transparent"
					anchors {
						top:parent.top
						bottom: parent.bottom
						left: itemNameRect.left
					}
				}
				Rectangle {
					id: itemImageRect
					height: parent.height
					width: parent.height
					visible: showImage
					radius: 8
					anchors {
						top:parent.top
						left: parent.left
					}
					color: "transparent"
					Image {
						id: itemImage
						asynchronous: true
						source: imageSourceTemplate.replace("%1", model.itemId)
						anchors.fill: parent
						fillMode: Image.PreserveAspectFit
					}
				}
				Rectangle {
					id: itemSymbol
					height: parent.height
					width: parent.height
					visible: noImage && !hideNoImageSymbol
					radius: 8
					anchors {
						top:parent.top
						left: parent.left
					}
					color: "transparent"
					Rectangle {
						height: parent.height/3
						width: parent.height/3
						color: "darkcyan"
						border.width: 3
						border.color: "darkgray"
						radius: width
						anchors.centerIn: parent
					}

				}
				Rectangle {
					id: itemNameRect
					color: "transparent"
					radius: 8
					anchors {
						top:parent.top
						bottom: parent.bottom
						left: showImage ? itemImageRect.right
						                : (reserveLeftSymbolSpace ? itemSymbol.right : parent.left)
						right: locked ? lockIcon.left
						              : (showEditPanel ? itemEditRect.left : parent.right)
						leftMargin: showImage || reserveLeftSymbolSpace ? 10 : 0
						rightMargin: (showEditPanel || locked) ? 10 : 0
					}
					Label {
						id: itemNameLabel
						anchors.fill: parent
						anchors.rightMargin: expandIcon.visible ? expandIcon.implicitWidth + 8 : 0
						text: model.itemName !== undefined && model.itemName !== null
						      ? String(model.itemName) : ""
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						wrapMode: Text.WordWrap
						font.bold: true
						font.pixelSize: itemList.itemFontPixelSize
						color: isSelected ? selectedTextColor : unselectedTextColor
					}

					Text {
						id: expandIcon
						anchors {
							right: parent.right
							verticalCenter: parent.verticalCenter
							rightMargin: 6
						}
						visible: itemList.showExpandIndicator && model.hasSubPrograms
						text: itemList.expandIndicatorActiveIndex === model.rowIndex ? "\u25BE" : "\u25B8"
						font.bold: true
						font.pixelSize: itemList.itemFontPixelSize
						color: isSelected ? selectedTextColor : unselectedTextColor
					}
					MouseArea {
						anchors.fill: parent
						enabled: !itemRoot.locked
						onClicked: itemList.selectIndex(index)
					}
				}
				Text {
					id: lockIcon
					visible: itemRoot.locked
					width: parent.height
					anchors {
						top: parent.top
						bottom: parent.bottom
						right: parent.right
					}
					text: "🔒"
					font.pixelSize: Math.round(itemList.itemFontPixelSize * 2.0)
					horizontalAlignment: Text.AlignHCenter
					verticalAlignment: Text.AlignVCenter
					color: itemRoot.isSelected ? selectedTextColor : unselectedTextColor
				}
				Rectangle {
					id: itemEditRect
					property bool engaged: false
					readonly property bool actionsVisible: itemList.alwaysShowEditActions || engaged
                    color: "#80FFFFFF"
                    border.color: "#AA4040C0"
					border.width: 1
					radius: 8
					width: (itemList.alwaysShowEditActions || !actionsVisible)
						   ? height
						   : 3 * height
					visible: showEditPanel
					clip: true
					anchors {
						top: parent.top
						bottom: parent.bottom
						right:  parent.right
						margins: 10
					}

					Button {
						id: directEditButton
						flat: true
						visible: itemList.alwaysShowEditActions
						anchors.fill: parent
						anchors.margins: 5
						implicitWidth: 1
						implicitHeight: 1
						contentItem: Text {
							text: "✎"
							font.pixelSize: Math.round(itemList.itemFontPixelSize * 1.4)
							horizontalAlignment: Text.AlignHCenter
							verticalAlignment: Text.AlignVCenter
							color: "#264093"
						}
						background: Item {}
						onClicked: {
							nameDialog.editingIndex = index
							nameDialog.initialName = (model.itemName !== undefined && model.itemName !== null)
							                            ? String(model.itemName) : ""
							nameDialog.open()
						}
					}

					Button {
						id: engageEditButton
						flat: true
						visible: !itemList.alwaysShowEditActions && !itemEditRect.actionsVisible
						anchors.fill: parent
						anchors.margins: 5
						implicitWidth: 1
						implicitHeight: 1
						contentItem: Text {
							text: "‹"
							font.pixelSize: Math.round(itemList.itemFontPixelSize * 1.8)
							horizontalAlignment: Text.AlignHCenter
							verticalAlignment: Text.AlignVCenter
							color: "#264093"
						}
						background: Item {}
						onClicked: {
							itemEditRect.engaged = true;
						}
					}
					Rectangle {
						id: editVariantBox
						visible: !itemList.alwaysShowEditActions && itemEditRect.actionsVisible
						color: "transparent"
						anchors.fill: parent
						anchors.margins: 5
						RowLayout {
							anchors.fill: parent
							spacing: 5
							anchors.margins: 0
							Button {
								id: deleteButton
								flat: true
								Layout.fillHeight: true
								Layout.fillWidth: true
								implicitWidth: 1
								implicitHeight: 1
								contentItem: Text {
									text: "🗑"
									font.pixelSize: Math.round(itemList.itemFontPixelSize * 1.3)
									horizontalAlignment: Text.AlignHCenter
									verticalAlignment: Text.AlignVCenter
								}
								background: Item {}
								onClicked: {
									deleteItem(index)
								}
							}
							Button {
								id: renameButton
								flat: true
								Layout.fillHeight: true
								Layout.fillWidth: true
								implicitWidth: 1
								implicitHeight: 1
								contentItem: Text {
									text: "✎"
									font.pixelSize: Math.round(itemList.itemFontPixelSize * 1.3)
									horizontalAlignment: Text.AlignHCenter
									verticalAlignment: Text.AlignVCenter
									color: "#264093"
								}
								background: Item {}
								onClicked: {
									nameDialog.editingIndex = index
									nameDialog.initialName = (model.itemName !== undefined && model.itemName !== null)
									                            ? String(model.itemName) : ""
									nameDialog.open()
								}
							}
							Button {
								id: cancelButton
								flat: true
								Layout.fillHeight: true
								Layout.fillWidth: true
								implicitWidth: 1
								implicitHeight: 1
								contentItem: Text {
									text: "›"
									font.pixelSize: Math.round(itemList.itemFontPixelSize * 1.8)
									horizontalAlignment: Text.AlignHCenter
									verticalAlignment: Text.AlignVCenter
									color: "#264093"
								}
								background: Item {}
								onClicked: {
									itemEditRect.engaged = false;
								}
							}
						}
					}
				}
				Rectangle {
					id: spacer
					height: 2
					width: parent.width
					gradient: Gradient.SolidStone
					opacity: 0.5
					anchors.bottom: parent.bottom
                    anchors.bottomMargin: -6
				}
			}
		}
	}

	Dialog {
		id: nameDialog
		property int editingIndex
		property string initialName: ""

		function ensureKeyboard() {
			if (!edit.activeFocus)
				edit.forceActiveFocus()
			Qt.inputMethod.show()
		}

		function submitRename() {
			var newName = edit.text.trim()
			edit.focus = false
			Qt.inputMethod.hide()
			if (newName.length > 0 && editingIndex >= 0) {
				editItemName(editingIndex, newName)
			}
			close()
		}
		function requestDelete() {
			var idx = editingIndex
			edit.focus = false
			Qt.inputMethod.hide()
			close()
			if (idx >= 0)
				Qt.callLater(function() { deleteItem(idx) })
		}
		width: Math.min(parent ? parent.width * 0.92 : 980, 980)
		height: 360
		parent: Overlay.overlay
		modal: true
		x: parent ? (parent.width - width) / 2 : 0
		// Всегда у верхнего края: кнопки остаются над виртуальной клавиатурой
        y: 80
		title: qsTr("Редактирование")
		Overlay.modal: Rectangle {
			color: "#70000000"
		}

		onOpened: {
			edit.text = initialName
			Qt.callLater(function() {
				edit.forceActiveFocus()
				edit.deselect()
				edit.cursorPosition = edit.text.length
				Qt.inputMethod.show()
			})
		}

		Connections {
			target: Qt.inputMethod
			enabled: nameDialog.visible
			function onVisibleChanged() {
				// После скрытия клавиатуры снимаем фокус, чтобы следующее
				// нажатие на поле снова активировало ввод и клавиатуру.
				if (!Qt.inputMethod.visible && edit.activeFocus)
					edit.focus = false
			}
		}
		contentItem: Rectangle {
			id: contentRect
			color: "white"

			ColumnLayout {
				anchors.fill: parent
				anchors.margins: 18
				spacing: 16

				Label {
					id: editLabel
					Layout.fillWidth: true
					horizontalAlignment: Qt.AlignCenter
					verticalAlignment: Qt.AlignVCenter
					text: qsTr("Укажите новое название:")
					color: "black"
					font.pixelSize: 34
					font.bold: true
					wrapMode: Text.WordWrap
				}

				TextField {
					id: edit
					Layout.fillWidth: true
					Layout.preferredHeight: 72
					color: "black"
					horizontalAlignment: Text.AlignLeft
					verticalAlignment: Text.AlignVCenter
					selectByMouse: true
					activeFocusOnPress: true
					inputMethodHints: Qt.ImhNoPredictiveText
					font.pixelSize: 32
					background: Rectangle {
						color: "#f5f5f5"
						border.color: edit.activeFocus ? "#4a9eff" : "#7a7a7a"
						border.width: 2
						radius: 6
					}
					onActiveFocusChanged: {
						if (activeFocus)
							Qt.inputMethod.show()
					}

					MouseArea {
						anchors.fill: parent
						propagateComposedEvents: true
						onPressed: {
							nameDialog.ensureKeyboard()
							mouse.accepted = false
						}
					}
				}
			}
		}
		footer: Rectangle {
			color: "transparent"
			implicitHeight: 120

			RowLayout {
				anchors.fill: parent
				anchors.leftMargin: 24
				anchors.rightMargin: 24
				anchors.topMargin: 16
				anchors.bottomMargin: 16
				spacing: 18

				DialogActionButton {
					Layout.preferredWidth: 220
					Layout.fillHeight: true
					text: qsTr("ОТМЕНА")
					labelPixelSize: 34
					onPressed: nameDialog.reject()
				}

				Item { Layout.fillWidth: true }

				DialogActionButton {
					Layout.preferredWidth: 220
					Layout.fillHeight: true
					text: qsTr("УДАЛИТЬ")
					labelPixelSize: 34
					onPressed: nameDialog.requestDelete()
				}

				Item { Layout.fillWidth: true }

				DialogActionButton {
					Layout.preferredWidth: 220
					Layout.fillHeight: true
					text: qsTr("ПРИНЯТЬ")
					primary: true
					labelPixelSize: 34
					enabled: edit.text.trim().length > 0
					onPressed: nameDialog.submitRename()
				}
			}
		}
		onAccepted: {
			nameDialog.submitRename()
		}
		onRejected: {
			edit.focus = false
			Qt.inputMethod.hide()
			close()
		}
		onClosed: {
			edit.focus = false
			Qt.inputMethod.hide()
			initialName = ""
			editingIndex = -1
			edit.text = ""
		}
	}
}
