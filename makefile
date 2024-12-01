ns:
	gcc -o ns NamingServer/ns.c -lpthread
	./ns

client:
	gcc -o cli Client/client.c -lpthread
	./cli

ss:
	gcc -o SS StorageServer/SS.c -lpthread
	./SS

clean:
	rm -f ns cli SS