package decoder

import (
	"context"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"net"

	"github.com/cloudflare/ebpf_exporter/v2/config"
)

type PodNamespace struct{}
type PodAZ struct{}

func getClientSet() *kubernetes.Clientset {
	clusterConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil
	}
	clientset, err := kubernetes.NewForConfig(clusterConfig)
	if err != nil {
		return nil
	}
	return clientset
}

func getPodByIP(clientset *kubernetes.Clientset, ip string) *v1.Pod {
	listOptions := metav1.ListOptions{
		FieldSelector: "status.podIP=" + ip,
	}
	pods, err := clientset.CoreV1().Pods("").List(context.TODO(), listOptions)
	if err != nil || len(pods.Items) != 1 {
		return nil
	}
	return &pods.Items[0]
}

func getNodeByName(clientset *kubernetes.Clientset, name string) *v1.Node {
	listOptions := metav1.ListOptions{
		FieldSelector: "metadata.name=" + name,
	}
	nodes, err := clientset.CoreV1().Nodes().List(context.TODO(), listOptions)
	if err != nil || len(nodes.Items) != 1 {
		return nil
	}
	return &nodes.Items[0]

}

func (p *PodNamespace) Decode(in []byte, _ config.Decoder) ([]byte, error) {
	ip := net.IP(in).String()
	clientset := getClientSet()

	if clientset == nil {
		return []byte("unknown"), nil
	}

	pod := getPodByIP(clientset, ip)
	if pod == nil {
		return []byte("unknown"), nil
	}

	return []byte(pod.GetNamespace()), nil
}

func (p *PodAZ) Decode(in []byte, _ config.Decoder) ([]byte, error) {
	ip := net.IP(in).String()
	clientset := getClientSet()

	if clientset == nil {
		return []byte("unknown"), nil
	}

	pod := getPodByIP(clientset, ip)
	if pod == nil {
		return []byte("unknown"), nil
	}
	nodeName := pod.Spec.NodeName
	node := getNodeByName(clientset, nodeName)
	if node == nil {
		return []byte("unknown"), nil
	}
	az, ok := node.Labels["topology.kubernetes.io/zone"]
	if !ok {
		return []byte("unknown"), nil
	}
	return []byte(az), nil
}
