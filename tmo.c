//********************************************************************************
//*** tmo.c example 
//*** Waitman Gobble waitman@waitman.net
//*** NO WARRANTIES
//***
//*** parallel Fetch and insert URLs into MongoDB
//*** ref: https://computing.llnl.gov/tutorials/parallel_comp/
//*** urls in argang.sources
//*** documents inserted into argang.fetched
//***
//*** build 

/*see build.sh for compile*/

//********************************************************************************


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include "mongo.h"
#include <curl/curl.h>
#include <libxml/xmlreader.h>

#define MAXT 200
#define MAX_BUF 65536

char *urls[MAXT];
char sequence_ID[25];
time_t lastModifiedDate;

//callback function for cURL fetch return size to libcurl
//userp contains url index

size_t static w_callback( void *buffer, size_t size, size_t nmemb, void *userp ) {

	int idx = ( intptr_t ) userp;
	int segsize = size * nmemb;

	int status;
	mongo conn[1];
	status = mongo_connect( conn, "127.0.0.1", 27017 );
	
	if ( status != MONGO_OK ) {
		//poof()
	} else {
		bson b[1];
		bson_init( b );
		bson_append_new_oid( b, "_id" );
		bson_append_string( b, "url", urls[idx] );
		bson_append_string( b, "sequence_ID", sequence_ID );
		bson_append_string( b, "content", buffer );
		bson_finish( b );
		mongo_insert( conn, "argang.fetched", b );
		bson_destroy( b );
	}
	return segsize;
	
}

//callback function for headers
//userp contains url index
//note: hdr_callback called with _each_ header, ie all headers individual strings not combined

size_t static hdr_callback( void *buffer, size_t size, size_t nmemb, void *userp ) {

	int idx = ( intptr_t ) userp;
	int segsize = size * nmemb;

	int status;
	mongo conn[1];
	status = mongo_connect( conn, "127.0.0.1", 27017 );
	
	if ( status != MONGO_OK ) {
		//poof()
	} else {
		bson b[1];
		bson_init( b );
		bson_append_new_oid( b, "_id" );
		bson_append_string( b, "url", urls[idx] );
		bson_append_string( b, "sequence_ID", sequence_ID );
		bson_append_string( b, "content", buffer );
		bson_finish( b );
		mongo_insert( conn, "argang.headers", b );
		bson_destroy( b );
	}
	return segsize;
	
}


//fetch url using cURL

static void *fetch_url( void *i ) {

	int idx = ( intptr_t ) i;
	CURL *curl;

	curl = curl_easy_init();
	if ( curl != NULL ) {
		CURLcode result;
		result = curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
		assert( result == CURLE_OK );
		result = curl_easy_setopt( curl, CURLOPT_FILETIME, 1 );
		assert( result == CURLE_OK );
		result = curl_easy_setopt( curl, CURLOPT_NOSIGNAL, 1 );
		assert( result == CURLE_OK ); 
		result = curl_easy_setopt( curl, CURLOPT_URL, urls[idx] );
		assert( result == CURLE_OK ); 
		result = curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, hdr_callback ); 
		assert( result == CURLE_OK ); 
		result = curl_easy_setopt( curl, CURLOPT_TIMEVALUE, lastModifiedDate ); 
		assert( result == CURLE_OK ); 
		result = curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, w_callback );
		assert( result == CURLE_OK ); 
		result = curl_easy_setopt( curl, CURLOPT_WRITEDATA, (void *) (intptr_t) idx );
		assert( result == CURLE_OK ); 
		result = curl_easy_perform( curl );
		assert( result == CURLE_OK );	
		curl_easy_cleanup( curl );
	} else {
		printf( "Error! Bummer! cURL not working!\n" );
	}
	return NULL;
}


static void process_node(xmlTextReaderPtr reader) {
	const xmlChar *name, *value;

	name = xmlTextReaderConstName(reader);
	value = xmlTextReaderConstValue(reader);
	if ( xmlStrcasecmp( name, BAD_CAST( "title" ))) {
		printf( "%s\n",value );
	}
}


static void stream_doc(const char *content, int length) {
	
	xmlDocPtr doc;
	doc = xmlReadMemory(content, length, "phantom.xml", NULL, 0);
	xmlFreeDoc(doc);
}

		
//load urls into urls[] and launch threadsed
//todo: watch for MAXT max threads and do something

int main( int argc, char **argv ) {
	
	LIBXML_TEST_VERSION

	pthread_t tid[MAXT];
	int i=0;
	int j=0;
	int error;

	int status;
	mongo conn[1];
	mongo_cursor cursor[1];
	bson_iterator iterator[1];
	
	curl_global_init( CURL_GLOBAL_ALL );
	
	status = mongo_connect( conn, "127.0.0.1", 27017 );

	if ( status != MONGO_OK ) {
		//poof()
	} else {
		
		fprintf( stderr, "Getting OID...\n" );
		bson b[1];
		bson_init( b );
		bson_append_new_oid( b, "_id" );
		
		bson_oid_t oid;
		bson_oid_gen( &oid );
		bson_append_oid( b, "_id", &oid );
		
		time_t epoch = time(NULL);
		fprintf( stderr, "Init Time %i\n", (int) epoch );
		bson_append_int( b, "init", (int) epoch );
		bson_finish( b );
		mongo_insert( conn, "argang.lastload", b );
		
		bson_oid_to_string( &oid, sequence_ID );
		fprintf( stderr, "Running %s OK.\n", sequence_ID );
		
		bson_destroy( b );

		fprintf( stderr, "Launching Threads...\n" );
		
		mongo_cursor_init( cursor, conn, "argang.sources" );
		while ( mongo_cursor_next( cursor ) == MONGO_OK ) {
			if ( bson_find( iterator, mongo_cursor_bson( cursor ), "url" ) ) {
				urls[i] = malloc( 512 * sizeof( char ) );
				sprintf( urls[i], "%s", bson_iterator_string( iterator ) );
				error = pthread_create( &tid[i], NULL, fetch_url,  ( void * ) ( intptr_t ) i );
				if ( error != 0 ) {
					fprintf( stderr, "Error on thread number %d, errno %d, url %s\n", i, error, urls[i] );
				}
				i++;
			}
		}
	}
	mongo_cursor_destroy( cursor );

	for ( j=0; j<i; j++ ) {
    		error = pthread_join( tid[j], NULL );
		fprintf( stderr, ".. %d .. ", j );
	}

	xmlCleanupParser();

	return (0);
}
