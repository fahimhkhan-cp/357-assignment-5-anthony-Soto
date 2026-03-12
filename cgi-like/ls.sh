echo "<html>"
echo "<head><title>CGI ls Test</title></head>"
echo "<body>"
echo "<h1>Listing for arguments: $@</h1>"
echo "<pre>"


ls "$@"

echo "</pre>"
echo "</body>"
echo "</html>"
