package network_project;

@license(LGPL);
//
// TODO documentation
//
network Network
{
    submodules:

        //        node: Node {
        //            @display("p=51,74");
        //        }
        //        node1: Node {
        //            @display("p=420,74");
        //        }
        nodes[2]: Node;
        coordinator: coordinator {
            @display("p=323,234");
        }
    connections:

        coordinator.out[0] --> nodes[0].in[0];
        coordinator.out[1] --> nodes[1].in[0];
        nodes[0].out --> nodes[1].in[1];
        nodes[1].out --> nodes[0].in[1];
}

