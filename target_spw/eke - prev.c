
#include "eke.h"

unsigned char * md5Hash(const char *str, int length) {
    int n;
    MD5_CTX c;
    unsigned char digest[16];
    unsigned char *out = (unsigned char*)malloc(33);

    MD5_Init(&c);

    while (length > 0) {
        if (length > 512) {
            MD5_Update(&c, str, 512);
        } else {
            MD5_Update(&c, str, length);
        }
        length -= 512;
        str += 512;
    }

    MD5_Final(digest, &c);

    for (n = 0; n < 16; ++n) {
        snprintf(&(out[n*2]), 16*2, "%02x", (unsigned int)digest[n]);
    }

    return out;
}

// calc a * b % p , avoid 64bit overflow
static inline uint64_t
mul_mod_p(uint64_t a, uint64_t b) {
	uint64_t m = 0;
	while(b) {
		if(b&1) {
			uint64_t t = P-a;
			if ( m >= t) {
				m -= t;
			} else {
				m += a;
			}
		}
		if (a >= P - a) {
			a = a * 2 - P;
		} else {
			a = a * 2;
		}
		b>>=1;
	}
	return m;
}

static inline uint64_t
pow_mod_p(uint64_t a, uint64_t b) {
	if (b==1) {
		return a;
	}
	uint64_t t = pow_mod_p(a, b>>1);
	t = mul_mod_p(t,t);
	if (b % 2) {
		t = mul_mod_p(t, a);
	}
	return t;
}

// calc a^b % p
uint64_t
powmodp(uint64_t a, uint64_t b) {
	if (a > P)
		a%=P;
	return pow_mod_p(a,b);
}

uint64_t
randomint64() {
	//TODO: check  if rand is rand? 
	uint64_t a = rand();
	uint64_t b = rand();
	uint64_t c = rand();
	uint64_t d = rand();
	return a << 48 | b << 32 | c << 16 | d;
}

uint32_t
randomint32() {
	uint32_t a = rand();
	uint32_t b = rand();
	return a << 16 | b;
}
/*
static void
test() {
	
	 uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	 uint8_t buffer[64] ={0};
	 uint8_t result[64] = {0};
	 uint8_t input[64] = {0};
	 
	printf("asdfasdfasdf\n");
	// user side
	char password[] = "testtest";
	char* hashedPass_user = md5Hash(password,strlen(password));		
	printf("hashedPass_user: %s\n",hashedPass_user);
	uint64_t user_random = randomint64();
	printf("user_random= %" PRIx64 "\n", user_random);
	uint64_t user_publickey = powmodp(G, user_random);
	memcpy(input,&user_publickey,sizeof(user_publickey));
	printf("user_publickey= %" PRIx64 "\n", user_publickey);
	printf("size of user_publickey is: %d\n", sizeof(user_publickey));
	printf("size of input is: %d\n", sizeof(input));

	AES128_CBC_encrypt_buffer(buffer, input, sizeof(input), hashedPass_user, iv);
	// sends to server username and encrypted user_publickey with hashedPass_user as the key
	
	// server side
	// find user password decrypt it from the password file 
	char* hashedPass_server = md5Hash(password,strlen(password));
	printf("hashedPass_server: %s\n",hashedPass_server);
	uint64_t server_random = randomint64();
	printf("server_random= %" PRIx64 "\n", server_random);
	uint64_t server_public = powmodp(G, server_random);
	printf("server_public= %" PRIx64 "\n", server_public);

	AES128_CBC_decrypt_buffer(result+0, buffer+0,  16, hashedPass_server, iv);
	AES128_CBC_decrypt_buffer(result+16, buffer+16, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result+32, buffer+32, 16, 0, 0);
	AES128_CBC_decrypt_buffer(result+48, buffer+48, 16, 0, 0);
	
	uint64_t decryptedKey = 0;
	memcpy(&decryptedKey,result,sizeof(uint64_t));
	printf("decrypted message: %" PRIx64 "\n",decryptedKey);

	
	uint64_t server_symetricKey = powmodp(decryptedKey,server_random);
	printf("server_symetricKey= %" PRIx64 "\n", server_symetricKey);
	uint32_t serverChallenge = randomint32();
	printf("(32bit)serverChallenge= %" PRIx64 "\n", serverChallenge);
	// sends to user the server challenge encrypted with  server_symetricKey and server_public encrypted with hashedPass_server
		
	// user side
	// decrypt server_public 
	uint64_t user_symetricKey = powmodp(server_public,user_random);
	printf("user_symetricKey= %" PRIx64 "\n", user_symetricKey);
	uint32_t userChallenge = randomint32();
	printf("(32bit)userChallenge= %" PRIx64 "\n", userChallenge);
	// decrypt serverChallenge
	uint64_t combinedChallange = userChallenge;
	combinedChallange = combinedChallange << 32 | serverChallenge;
	printf("combinedChallange= %" PRIx64 "\n", combinedChallange);
	// send to server combinedChallagne encryted with user_symetricKey

	// server side
	// decrypt combinedChallagne
	uint64_t userChallengeAck = combinedChallange >> 32;
	printf("(32bit)userChallengeAck= %" PRIx64 "\n", userChallengeAck);
	// send to user userChallengeAck encrypted with server_symetricKey
	
	// continue session with symetricKey

//	assert(secret1 == secret2);
//	printf("a=%I64x b=%I64x s=%I64x\n", a,b,secret1);
	free(hashedPass_user);
	free(hashedPass_server);
}

void printBits(uint64_t num)
{
   for(int bit=0;bit<(sizeof(uint64_t) * 8); bit++)
   {
      printf("%i", num & 0x01);
      num = num >> 1;
   }
    printf("\n");
}
*/

