#!/usr/bin/python3
# -*- coding: utf-8 -*-
__license__ = """
  Copyright (c) 2015 Pontus Sköldström, Bertrand Pechenot

  This file is part of libdd, the DoubleDecker hierarchical
  messaging system DoubleDecker is free software; you can
  redistribute it and/or modify it under the terms of the GNU Lesser
  General Public License (LGPL) version 2.1 as published by the Free
  Software Foundation.

  As a special exception, the Authors give you permission to link this
  library with independent modules to produce an executable,
  regardless of the license terms of these independent modules, and to
  copy and distribute the resulting executable under terms of your
  choice, provided that you also meet, for each linked independent
  module, the terms and conditions of the license of that module. An
  independent module is a module which is not derived from or based on
  this library.  If you modify this library, you must extend this
  exception to your version of the library.  DoubleDecker is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
  License for more details.  You should have received a copy of the
  GNU Lesser General Public License along with this program.  If not,
  see <http://www.gnu.org/licenses/>.
"""

import argparse
import logging
from doubledecker.clientSafe import ClientSafe
import docker
import docker.errors
import sys, time, json
import logging
from jsonrpcserver import dispatch, Methods
import jsonrpcserver.exceptions

from jsonrpcclient.request import Request, Notification
Request.notification_errors = True
from measure import MEASUREParser
from papbackend import PAPMeasureBackend
restart = False

# How to test with UN
#
#Pontus Sköldström: switch to the monitor-plugin branch, compile, run ./un-orchestrator with some parameters?
#Ivano Cerrato: yes, you must enable the compilation flags: ENABLE_DOUBLE_DECKER_CONNECTION
#Ivano Cerrato: and ENABLE_UNIFY_MONITORING_CONTROLLER
#Ivano Cerrato: then you have to specify the measure string in the NF-FG you give to the un-orchestrator



MeasureString = """ measurements {
  m1 = overload.risk.rx(interface = eth0);
  m2 = overload.risk.tx(interface = eth0);
  m3 = overload.risk.rx(interface = eth2);
  m4 = overload.risk.rx(interface = eth5);
  m5 = overload.risk.tx(interface = eth5);
  m6 = rate.tx(interface = eth5);
}
zones {
 z1 = (AVG(val = m1, max_age = \"5 minute\") < 0.5);
 z2 = (AVG(val = m1, max_age = \"5 minute\") > 0.5);
 z3 = ((AVG(val = m2, max_age = \"5 minute\") + AVG(val = m3, max_age = \"1 minute\"))   > 0.5);

}
actions {
 z1->z2 = Publish(topic = \"alarms\", message = \"z1 to z2\"); Notify(target = \"alarms\", message = \"z1 to z2\");
 z2->z1 = Publish(topic = \"alarms\", message = \"z2 to z\");
 ->z1 = Publish(topic = \"alarms\", message = \"entered z1\");
 z1-> = Publish(topic = \"alarms\", message = \"left z1\");
 z1 = Publish(topic = \"alarms\", message = \"in z1\");
 z2 = Publish(topic = \"alarms\", message = \"in z2\");
}"""


from pprint import pprint



class SecureCli(ClientSafe):
    def __init__(self, name, dealerurl, customer, keyfile):
        super().__init__(name, dealerurl, customer, keyfile)
        #pyjsonrpc.JsonRpc.__init__()
        self.docker = docker.Client(base_url='unix://var/run/docker.sock')
        self.MEASURE = None
        self.cmds = dict()
        self.cmds['pipelinedb'] = {'image':'gitlab.testbed.se:5000/pipelinedb', 'name':'pipelinedb' }
        self.cmds['aggregator'] = {'image':'aggregator', 'name':'aggregator'}
        self.cmds['opentsdb'] = {'image':'gitlab.testbed.se:5000/opentsdb', 'name':'opentsdb' }

        # Log the JSON-RPC messages, can be skipped
        logging.getLogger('jsonrpcserver').setLevel(logging.ERROR)

        self.logger = logging.getLogger("MMP")

        # Initialize the RPC-Server dispatcher
        self.methods = Methods()
        self.methods.add_method(self.updateNFFG)
        self.methods.add_method(self.docker_information_request)
        self.methods.add_method(self.ping)
        self.methods.add_method(self.testNFFG)


        # TODO: Add aggregator to this
        # Start the default containers (Pipeline, OpenTSDB, and Aggregator)
        self.initialize_containers()


    # RPC server
    def docker_information_request(self, ddsrc, name):
        self.logger.info("Retrieving information about container %s"%name)
        try:
            data = self.docker.inspect_container(name)
        except docker.errors.NotFound as e:
            raise jsonrpcserver.exceptions.ServerError(data=e.explanation)

        return data

    def add(self, a, b):
        """Test method"""
        print("JSON-RPC method self.add called")
        return a + b

    def sub(self,a,b):
        """Test method"""
        print("JSON-RPC method self.sub called")
        return a - b

    def testNFFG(self,ddsrc):
        self.logger.info("Got testNFFG call from %s"%ddsrc)
        parser = MEASUREParser()
        measure = parser.parseToDict(MeasureString)
        pap = PAPMeasureBackend()
        result = pap.generate_config(measure)
        pprint(result)
        self.publish(topic="aggregator", message=str(Request("configure",**result)))
        return "OK"

    def updateNFFG(self, ddsrc, nffg):
        print("updateNFFG called, with NFFG : ", nffg)
        self.MEASURE = nffg['MEASURE']
        self.vnfmapping = nffg['VNFs']

        print("VNF ID 1: ", self.vnf_to_name(1))
        print("VNF ID 2: ", self.vnf_to_name(2))
        print("VNF ID 3: ", self.vnf_to_name(3))
        print("VNF(Dock.. ): ", self.name_to_vnfid("docker-983249873294"))

        print("Port(1,1): ", self.port_to_name(1,1))
        print("Port(1,2): ", self.port_to_name(1,2))
        print("Port(1,3): ", self.port_to_name(1,3))
        print("Port(2,1): ", self.port_to_name(2,1))
        print("Port(2,2): ", self.port_to_name(2,2))
        print("Port(2,3): ", self.port_to_name(2,3))

        print("Port(veth0): ", self.name_to_port('veth0'))
        return self.vnf_to_name(1)


    # Mapping from NF-FG IDs to real names
    def name_to_vnfid(self, name):
        for n in self.vnfmapping:
            if n['name'] == name:
                return n['id']
        return None

    def name_to_port(self,port):
        for n in self.vnfmapping:
            vnfid = n['id']
            for p in n['ports']:
                if p['name'] == port:
                    return (vnfid,p['id'])
        return None

    def vnf_to_name(self, vnfid):
        for n in self.vnfmapping:
            if n['id'] == vnfid:
                return n['name']
        return None

    def port_to_name(self,vnfid, portid):
        for n in self.vnfmapping:
            if n['id'] == vnfid:
                for p in n['ports']:
                    if p['id'] == portid:
                        return p['name']
        return None


    def ping(self, ddsrc):
        return "OK"

    # Docker stuff
    def initialize_containers(self):
        containers = self.docker.containers(all=True)
        idlist = list()
        for c in containers:
            if any(ext in c['Names'][0] for ext in self.cmds.keys()):
                idlist.append(c['Names'][0])


        images = list()
        for img in self.docker.images():
           images.append(img['RepoTags'][0])


        # Pull opentsdb if not available
        #if self.cmds['opentsdb']['image'] not in images:
        #    for line in self.docker.pull(repository=self.cmds['opentsdb']['image'], stream=True):
        #        print("\r",json.dumps(json.loads(line.decode()), indent=4))
        # Pull piplinedb if not available
        #if self.cmds['pipelinedb']['image'] not in images:
        #    for line in self.docker.pull(repository=self.cmds['pipelinedb']['image'], stream=True):
        #        print("\r",json.dumps(json.loads(line.decode()), indent=4))


        #stop pipeline
        if restart:
            try:
                self.docker.stop(self.cmds['pipelinedb']['name'])
            except docker.errors.APIError as e:
                print("Error ", e , " while trying to stop PipelineDB")

        #stop OpenTSDB
            try:
                self.docker.stop(self.cmds['opentsdb']['name'])
            except docker.errors.APIError as e:
                print("Error ", e , " while trying to stop OpenTSDB",)

             #remove pipeline
            try:
                self.docker.remove_container(self.cmds['pipelinedb']['name'])
            except docker.errors.APIError as e:
                print("Error ", e , " while trying to remove PipelineDB")

            #remove OpenTSDB
            try:
                self.docker.remove_container(self.cmds['opentsdb']['name'])
            except docker.errors.APIError as e:
                print("Error ", e , " while trying to remove OpenTSDB")



        pipeline_ip = None
        #start pipeline
        if 'pipelinedb' not in idlist:
            try:
                print("Creating PipelineDB")
                cont = self.docker.create_container(**self.cmds['pipelinedb'])
                response = self.docker.start(container=cont.get('Id'))
                print("Result: ",response )

            except docker.errors.APIError as e:
                print("Error ", e , " while trying to create PipelineDB")
        try:
            pipeline_ip = self.docker.inspect_container('pipelinedb')['NetworkSettings']['IPAddress']
        except docker.errors.APIError as e:
            print("Error ", e , " while trying to create PipelineDB")

        opentsdb_ip = None
        #start OpenTSDB
        #try:
        #    print("Creating OpenTSDB")
        #    cont = self.docker.create_container(**self.cmds['opentsdb'])
        #    response = self.docker.start(container=cont.get('Id'))
        #    print("Result: ",response )
        #    opentsdb_ip = self.docker.inspect_container(cont['Id'])['NetworkSettings']['IPAddress']

#        except docker.errors.APIError as e:
#            print("Error ", e , " while trying to create OpenTSDB")

        # temp!
        opentsdb_up = True

        wait_time = 100
        while wait_time > 0:
            pipeline_up = self.check_server(pipeline_ip,5432)
       #     opentsdb_up = self.check_server(opentsdb_ip,4242)
            if pipeline_up and opentsdb_up:
                print("PipelineDB and OpenTSDB running!")
                return
            else:
                status_str = "Waiting %d for"%wait_time
                if not pipeline_up:
                    status_str += " PipelineDB "
                if not opentsdb_up:
                    status_str += " OpenTSDB"
                print(status_str)
                time.sleep(1)
            wait_time -= 1


    def check_server(self,address, port):
        import socket
        # Create a TCP socket
        s = socket.socket()
        print("Attempting to connect to %s on port %s" % (address, port))
        try:
            s.connect((address, port))
            print("Connected to %s on port %s" % (address, port))
            s.close()
            return True
        except socket.error as e:
            print("Connection to %s on port %s failed: %s" % (address, port, e))
            return False
    def handle_jsonrpc(self, src, msg,topic=None):
        request = json.loads(msg.decode('UTF-8'))


        if 'error' in request:
            logging.error("Got error response from: %s"%src)
            logging.error(str(request['error']))
            return

        if 'result' in request:
            logging.info("Got response from %s"%src)
            logging.info(str(request['result']))
            return

        # include the 'ddsrc' parameter so the
        # dispatched method knows where the message came from
        if 'params' not in request:
            request['params'] = {}

        request['params']['ddsrc']  = src.decode()
        response = dispatch(self.methods, request)

        # if the http_status is 200, its request/response, otherwise notification
        if response.http_status == 200:
            logging.info("Replying to %s with %s"%(str(src), str(response)))
            self.sendmsg(src,str(response))
        # notification, correctly formatted
        elif response.http_status == 204:
            pass
        # if 400, some kind of error
        # return a message to the sender, even if it was a notification
        elif response.http_status == 400:
            self.sendmsg(src,str(response))
            logging.error("Recived bad JSON-RPC from %s, error %s"%(str(src), str(response)))
        else:
            logging.error("Recived bad JSON-RPC from %s \nRequest: %s\nResponose: %s"%(str(src), msg.decode(),str(response)))

    # callback called automatically everytime a point to point is sent at
    # destination to the current client
    def on_data(self, src, msg):
        self.handle_jsonrpc(src=src, msg=msg)

    # callback called when the client receives a message on a topic he
    # subscribed to previously
    def on_pub(self, src, topic, msg):
        self.handle_jsonrpc(src=src,topic=topic, msg=msg)

    # callback called upon registration of the client with its broker
    def on_reg(self):
        self.logger.info("The client is now connected")
        topic = 'unify:mmp'
        scope = 'node'
        # this function notifies the broker that the client is interested
        # in the topic 'monitoring' and the scope should be 'all'
        self.logger.info("Subscribing to topic '%s', scope '%s'"%(topic,scope))
        self.subscribe(topic, scope)
       # self.logger.info("Subscribing to topic 'measurement', scope 'node'")
       # self.subscribe('measurement','node')


    # callback called when the client detects that the heartbeating with
    # its broker has failed, it can happen if the broker is terminated/crash
    # or if the link is broken
    def on_discon(self):
        self.logger.warning("The client got disconnected")

        # this function shuts down the client in a clean way
        # in this example it exists as soon as the client is disconnected
        # fron its broker

    # callback called when the client receives an error message
    def on_error(self, code, msg):
        print("ERROR n#%d : %s" % (code, msg))



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Generic message client")
    parser.add_argument(
        '-d',
        "--dealer",
        help='URL to connect DEALER socket to, "tcp://1.2.3.4:5555"',
        nargs='?',
        default='tcp://127.0.0.1:5555')
    parser.add_argument(
        '-f',
        "--logfile",
        help='File to write logs to',
        nargs='?',
        default=None)
    parser.add_argument(
        '-l',
        "--loglevel",
        help='Set loglevel (DEBUG, INFO, WARNING, ERROR, CRITICAL)',
        nargs='?',
        default="INFO")
    parser.add_argument(
        '-k',
        "--keyfile",
        help='File containing the encryption/authentication keys)',
        nargs='?',
        default='/etc/doubledecker/a-keys.json')

    args = parser.parse_args()

    numeric_level = getattr(logging, args.loglevel.upper(), None)
    if not isinstance(numeric_level, int):
        raise ValueError('Invalid log level: %s' % args.loglevel)

    logging.basicConfig(format='%(levelname)s:%(message)s', filename=args.logfile, level=numeric_level)


    logo =    r"""
   _____                .__  __               .__
  /     \   ____   ____ |__|/  |_  ___________|__| ____    ____
 /  \ /  \ /  _ \ /    \|  \   __\/  _ \_  __ \  |/    \  / ___\
/    Y    (  <_> )   |  \  ||  | (  <_> )  | \/  |   |  \/ /_/  >
\____|__  /\____/|___|  /__||__|  \____/|__|  |__|___|  /\___  /
        \/            \/                              \//_____/
   _____                                                             __
  /     \ _____    ____ _____     ____   ____   _____   ____   _____/  |_
 /  \ /  \\__  \  /    \\__  \   / ___\_/ __ \ /     \_/ __ \ /    \   __\
/    Y    \/ __ \|   |  \/ __ \_/ /_/  >  ___/|  Y Y  \  ___/|   |  \  |
\____|__  (____  /___|  (____  /\___  / \___  >__|_|  /\___  >___|  /__|
        \/     \/     \/     \//_____/      \/      \/     \/     \/
__________.__               .__
\______   \  |  __ __  ____ |__| ____
 |     ___/  | |  |  \/ ___\|  |/    \
 |    |   |  |_|  |  / /_/  >  |   |  \
 |____|   |____/____/\___  /|__|___|  /
                    /_____/         \/   """

    logging.info(logo)
    genclient = SecureCli(name="mmp",
                          dealerurl=args.dealer,
                          customer="a",
                          keyfile=args.keyfile)

    genclient.start()




# to be handled later
#
#
# def configure():
#     from pprint import pprint
#     monitorconfig = {}
#     monitorconfig['prepare'] = None
#     monitorconfig['evaluate'] = [{'select':'select risk from view_all where risk > 0.7;',
#                                   'action':'Publish(topic="alarm", message="Overload")'},
#                                  {'select':'select throughput from view_all where throughput > 0.1;',
#                                   'action':'Publish(topic="alarm", message="overflow ...")'}]
#     monitorconfig['MFs'] = {'m1': {'prepare':'CREATE STREAM stream_m1 (lm float, lsd float); '
#                    'CREATE CONTINOUS VIEW view_m1 as select AVG(lm) as lm, AVG(lsd) as lsd from stream_m1; ',
#                                    'insert': 'insert into stream_m1 (lm, lsd) VALUES (%s,%s)%result[]',
#                                    'evaluate': [{'select':'select risk from view_m1 where risk > 0.7;',
#                                   'action':'Publish(topic="alarm", message="Overload")'},
#                                  {'select':'select throughput from view_m1 where throughput > 0.1;',
#                                   'action':'Publish(topic="alarm", message="overflow ...")'}]
#                                    },
#                             'm2': {'prepare':'CREATE STREAM stream_m2 (lm float, lsd float); '
#                             'CREATE CONTINOUS VIEW view_m2 as select AVG(lm) as lm, AVG(lsd) as lsd from stream_m2; ',
#                                    'insert': 'insert into stream_m2 (lm, lsd) VALUES (%s,%s)%result[]',
#                                    'evaluate': [{'select':'select risk from view_m2 where risk > 0.7;',
#                                   'action':'Publish(topic="alarm", message="Overload")'},
#                                  {'select':'select throughput from view_m2 where throughput > 0.1;',
#                                   'action':'Publish(topic="alarm", message="overflow ...")'}]
#                                    }
#                             }
#     monitorconfig['postpare'] = 'CREATE CONTINOUS VIEW view_all as SELECT AVG(m1,m2,m3,m4) from view_m1, view_m2..);'
