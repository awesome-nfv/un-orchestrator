#include "resource_manager.h"

void ResourceManager::publishDescriptionFromFile(char *descr_file)
{
	assert(descr_file != NULL);

	FILE *fp = fopen(descr_file, "rb");
	if(fp == NULL) {
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Something wrong while opening file '%s'.",descr_file);
		fclose(fp);
		return;
	}
	fseek(fp, 0, SEEK_END);
	long fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);  //same as rewind(f);

	char *mesg = (char *) malloc(fsize + 1);
	if(fread(mesg, fsize, 1, fp) != (size_t) fsize) {
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "Something wrong while reading file '%s'.",descr_file);
		free(mesg)
		fclose(fp);
		return;
	}
	fclose(fp);

	mesg[fsize] = 0;

	/* XXX: who has to free mesg? */

	//publish the domain description
	DoubleDeckerClient::publish(FROG_DOMAIN_DESCRIPTION, mesg);
}

//TODO currently not used. It must be ported to use the new DD client
#if 0
//Export Domain Information
bool ResourceManager::publishUpdating()
{
	char c, *mesg = "";

	FILE *fp = fopen(FILE_NAME, "r");
	if(fp == NULL)
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "ERROR reading file.");

	int i = 0, n = 0;
	while(fscanf(fp, "%c", &c) != EOF){
		i++;
	}

	n = i;

	mesg = (char *)calloc(n, sizeof(char));

	fclose(fp);

	fp = fopen(FILE_NAME, "r");
	if(fp == NULL)
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__, "ERROR reading file.");

	for(i=0;i<n;i++){
		fscanf(fp, "%c", &c);
		mesg[i] = c;
	}

	mesg[i-1] = '\0';

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Publishing node configuration.");

	fclose(fp);

	//publish NF-FG
	client->publish("NF-FG", mesg, strlen(mesg), client);

	return true;
}
#endif
