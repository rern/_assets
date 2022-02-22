String dataFile = "raudio-ip.txt";
String ipSaved = "192.168.1.";
private void writeToFile( String data,Context context ) {
    try {
        OutputStreamWriter outputStreamWriter = new OutputStreamWriter( context.openFileOutput( dataFile, Context.MODE_PRIVATE ) );
        outputStreamWriter.write( data );
        outputStreamWriter.close();
    }
    catch ( IOException e ) {
        e.printStackTrace();
    }
}
private String readFromFile( Context context ) {
    try {
        InputStream inputStream = context.openFileInput( dataFile );
        if ( inputStream != null ) {
            InputStreamReader inputStreamReader = new InputStreamReader( inputStream );
            BufferedReader bufferedReader = new BufferedReader( inputStreamReader );
            String receiveString;
            StringBuilder stringBuilder = new StringBuilder();
            while ( ( receiveString = bufferedReader.readLine() ) != null ) {
                stringBuilder.append("\n").append(receiveString);
            }
            inputStream.close();
            ipSaved = stringBuilder.toString();
        }
    } catch ( IOException e ) {
        e.printStackTrace();
    }
    return ipSaved;
}

