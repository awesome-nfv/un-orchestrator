#include "resource_manager.h"
#include "description_parser.h"
#include "domain_description/domain_description.h"

void ResourceManager::publishDescriptionFromFile(char *descr_file)
{
	assert(descr_file != NULL);

	string fileContent;

	readDescriptionFromFile(descr_file, fileContent);

	domainInformations::DomainDescription *domainDescription = new domainInformations::DomainDescription();

	DescriptionParser::parseDescription(fileContent, domainDescription);

	Object json = domainDescription->toJSON();
	stringstream ssj;
	write_formatted(json, ssj);

	string mesg = ssj.str();

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Description to publish: %s", mesg.c_str());

	logger(ORCH_DEBUG_INFO, MODULE_NAME, __FILE__, __LINE__, "Publishing node configuration.");

	//publish the domain description
	DoubleDeckerClient::publish(FROG_DOMAIN_DESCRIPTION, mesg.c_str());
}

bool ResourceManager::readDescriptionFromFile(char *filename, string &fileContent) {
	logger(ORCH_INFO, MODULE_NAME, __FILE__, __LINE__,
		   "Considering the domain informations in file '%s'", filename);

	std::ifstream file;
	file.open(filename);
	if (file.fail()) {
		logger(ORCH_ERROR, MODULE_NAME, __FILE__, __LINE__,
			   "Cannot open the file %s", filename);
		return false;
	}

	stringstream stream;
	string str;
	while (std::getline(file, str))
		stream << str << endl;

	fileContent=stream.str();

	return true;
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
